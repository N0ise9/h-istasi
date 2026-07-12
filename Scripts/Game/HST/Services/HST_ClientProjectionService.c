class HST_MarkerProjectionSessionState
{
	int m_iPlayerId;
	bool m_bReady;
	bool m_bAwaitingSnapshotAcknowledge;
	bool m_bAwaitingDeltaAcknowledge;
	bool m_bAllowImmediateSnapshotRestart;
	bool m_bForceSnapshot;
	int m_iEpoch;
	int m_iAcknowledgedSequence;
	int m_iLastSentSequence;
	int m_iSnapshotWatermark;
	string m_sSnapshotId;
	string m_sExpectedAcknowledgeHash;
	string m_sLastReason;
	int m_iLastReadyRequestTick;
	int m_iLastResyncRequestTick;
	int m_iLastRejectedAcknowledgeTick;
	int m_iLastSnapshotDispatchTick;
	int m_iInFlightDispatchTick;
}

// Server-owned marker projection stream. It is intentionally limited to the
// campaign marker aggregate; menu, task, notification, and player-marker state
// remain separate contracts until their own cutovers.
class HST_ClientProjectionService
{
	static const int MAX_JOURNAL_EVENTS = 512;
	static const int REQUEST_COOLDOWN_MS = 750;
	static const int SNAPSHOT_RESTART_COOLDOWN_MS = 5000;
	static const int MAX_CLIENT_HASH_CHARACTERS = 128;
	static const int MAX_CLIENT_REASON_CHARACTERS = 120;

	protected ref map<string, ref HST_MapMarkerState> m_mCurrentRecords = new map<string, ref HST_MapMarkerState>();
	protected ref array<ref HST_MapMarkerState> m_aJournal = {};
	protected ref map<int, ref HST_MarkerProjectionSessionState> m_mSessions = new map<int, ref HST_MarkerProjectionSessionState>();
	protected bool m_bInitialized;
	protected int m_iEpoch = 1;
	protected int m_iCurrentSequence;
	protected int m_iLastObservedSequence;
	protected int m_iSnapshotSequence;
	protected string m_sCurrentRegistryHash = "0";
	protected string m_sLastSynchronizationReason = "not synchronized";

	void Synchronize(HST_CampaignState state, string reason = "marker rebuild")
	{
		if (!state)
			return;

		int epoch = Math.Max(1, state.m_iMarkerProjectionEpoch);
		int sequence = Math.Max(0, state.m_iMarkerProjectionSequence);
		if (m_bInitialized && epoch < m_iEpoch)
		{
			epoch = m_iEpoch + 1;
			state.m_iMarkerProjectionEpoch = epoch;
		}
		if (!m_bInitialized || epoch != m_iEpoch)
		{
			bool changedEpoch = m_bInitialized && epoch != m_iEpoch;
			m_bInitialized = true;
			m_iEpoch = epoch;
			m_iCurrentSequence = sequence;
			m_iLastObservedSequence = sequence;
			m_aJournal.Clear();
			CaptureCurrentRecords(state);
			if (changedEpoch)
				ForceAllSessionsToSnapshot("projection epoch changed");
			m_sLastSynchronizationReason = reason + " | initialized";
			return;
		}

		if (sequence < m_iLastObservedSequence)
		{
			m_iEpoch = Math.Max(m_iEpoch + 1, epoch + 1);
			state.m_iMarkerProjectionEpoch = m_iEpoch;
			m_iCurrentSequence = sequence;
			m_iLastObservedSequence = sequence;
			m_aJournal.Clear();
			CaptureCurrentRecords(state);
			ForceAllSessionsToSnapshot("projection sequence regressed");
			m_sLastSynchronizationReason = reason + " | sequence regression isolated by new epoch";
			return;
		}

		bool journalGap;
		array<ref HST_MapMarkerState> newEvents = {};
		for (int expectedSequence = m_iLastObservedSequence + 1; expectedSequence <= sequence; expectedSequence++)
		{
			HST_MapMarkerState eventRecord = FindRecordBySequence(state, expectedSequence);
			if (!eventRecord)
			{
				journalGap = true;
				break;
			}
			newEvents.Insert(HST_MarkerProjectionCodec.CopyMarker(eventRecord));
		}

		if (journalGap || !StateMatchesExpectedProjection(state, newEvents))
		{
			m_iEpoch = Math.Max(m_iEpoch + 1, epoch + 1);
			state.m_iMarkerProjectionEpoch = m_iEpoch;
			m_iCurrentSequence = sequence;
			m_iLastObservedSequence = sequence;
			m_aJournal.Clear();
			CaptureCurrentRecords(state);
			ForceAllSessionsToSnapshot("projection registry discontinuity");
			m_sLastSynchronizationReason = reason + " | registry discontinuity isolated by new epoch";
			return;
		}

		foreach (HST_MapMarkerState newEvent : newEvents)
			m_aJournal.Insert(newEvent);
		m_iCurrentSequence = sequence;
		m_iLastObservedSequence = sequence;
		CaptureCurrentRecords(state);
		m_sLastSynchronizationReason = reason;
		PruneJournal();
	}

	HST_MarkerProjectionDispatch RegisterReady(
		int playerId,
		int protocolVersion,
		int knownEpoch,
		int knownSequence,
		string knownRegistryHash,
		string reason = "client ready")
	{
		HST_MarkerProjectionDispatch dispatch = new HST_MarkerProjectionDispatch();
		if (!m_bInitialized || playerId <= 0)
		{
			dispatch.m_sReason = "projection service or player identity not ready";
			return dispatch;
		}
		if (knownRegistryHash.Length() > MAX_CLIENT_HASH_CHARACTERS)
		{
			dispatch.m_sReason = "client registry hash exceeds protocol limit";
			return dispatch;
		}
		if (protocolVersion != HST_MarkerProjectionCodec.PROTOCOL_VERSION)
		{
			dispatch.m_sReason = "protocol mismatch";
			return dispatch;
		}

		HST_MarkerProjectionSessionState session = ResolveSession(playerId);
		knownSequence = Math.Max(0, knownSequence);
		int nowTick = System.GetTickCount();
		if (session.m_bAwaitingSnapshotAcknowledge || session.m_bAwaitingDeltaAcknowledge)
		{
			int inFlightWatermark = session.m_iLastSentSequence;
			if (session.m_bAwaitingSnapshotAcknowledge)
				inFlightWatermark = session.m_iSnapshotWatermark;
			if (knownEpoch == session.m_iEpoch
				&& knownSequence == inFlightWatermark
				&& knownRegistryHash == session.m_sExpectedAcknowledgeHash)
			{
				session.m_bAwaitingSnapshotAcknowledge = false;
				session.m_bAwaitingDeltaAcknowledge = false;
				session.m_bForceSnapshot = false;
				session.m_iAcknowledgedSequence = knownSequence;
				session.m_iInFlightDispatchTick = 0;
				session.m_sSnapshotId = "";
				session.m_iLastReadyRequestTick = nowTick;
				session.m_sLastReason = "readiness confirmed in-flight watermark";
				PruneJournal();
				return BuildPendingDispatch(playerId, "readiness recovered lost acknowledgement");
			}
			if (IsRequestRateLimited(session.m_iLastReadyRequestTick, nowTick))
			{
				dispatch.m_sReason = "projection acknowledgement pending";
				return dispatch;
			}
			if (IsInFlightRestartRateLimited(session, nowTick))
			{
				dispatch.m_sReason = "in-flight snapshot restart rate limited";
				return dispatch;
			}
			session.m_iLastReadyRequestTick = nowTick;
			session.m_bReady = true;
			ForceSessionToSnapshot(session, "readiness restarted incomplete in-flight dispatch");
			return BuildSnapshotDispatch(session, "readiness retransmit snapshot");
		}
		if (!session.m_bForceSnapshot && IsRequestRateLimited(session.m_iLastReadyRequestTick, nowTick))
		{
			dispatch.m_sReason = "client readiness request rate limited";
			return dispatch;
		}
		session.m_iLastReadyRequestTick = nowTick;
		session.m_bReady = true;
		session.m_bAwaitingSnapshotAcknowledge = false;
		session.m_bAwaitingDeltaAcknowledge = false;
		session.m_bForceSnapshot = false;
		session.m_iEpoch = m_iEpoch;
		session.m_iAcknowledgedSequence = 0;
		session.m_iLastSentSequence = 0;
		session.m_sSnapshotId = "";
		session.m_sExpectedAcknowledgeHash = "";
		session.m_sLastReason = SanitizeClientText(reason, MAX_CLIENT_REASON_CHARACTERS);

		if (knownEpoch == m_iEpoch && knownSequence == m_iCurrentSequence && knownRegistryHash == m_sCurrentRegistryHash)
		{
			session.m_iAcknowledgedSequence = knownSequence;
			dispatch.m_iEpoch = m_iEpoch;
			dispatch.m_iFromSequence = knownSequence;
			dispatch.m_iToSequence = knownSequence;
			dispatch.m_sRegistryHash = m_sCurrentRegistryHash;
			dispatch.m_sReason = "client registry already current";
			PruneJournal();
			return dispatch;
		}

		if (knownEpoch == m_iEpoch && knownSequence < m_iCurrentSequence && CanReplayFrom(knownSequence + 1))
		{
			session.m_iAcknowledgedSequence = knownSequence;
			return BuildDeltaDispatch(session, knownSequence + 1, "ready delta replay");
		}

		return BuildSnapshotDispatch(session, "ready snapshot");
	}

	HST_MarkerProjectionDispatch BuildPendingDispatch(int playerId, string reason = "marker mutation")
	{
		HST_MarkerProjectionDispatch dispatch = new HST_MarkerProjectionDispatch();
		HST_MarkerProjectionSessionState session = m_mSessions.Get(playerId);
		if (!session || !session.m_bReady)
		{
			dispatch.m_sReason = "client projection session not ready";
			return dispatch;
		}
		if (session.m_bAwaitingSnapshotAcknowledge)
		{
			dispatch.m_sReason = "snapshot acknowledgement pending";
			return dispatch;
		}
		if (session.m_bAwaitingDeltaAcknowledge)
		{
			dispatch.m_sReason = "delta acknowledgement pending";
			return dispatch;
		}
		if (session.m_bForceSnapshot || session.m_iEpoch != m_iEpoch)
			return BuildSnapshotDispatch(session, reason + " | forced snapshot");
		if (session.m_iAcknowledgedSequence >= m_iCurrentSequence)
		{
			dispatch.m_sReason = "client already current";
			return dispatch;
		}

		int fromSequence = session.m_iAcknowledgedSequence + 1;
		if (!CanReplayFrom(fromSequence))
			return BuildSnapshotDispatch(session, reason + " | retained journal unavailable");
		return BuildDeltaDispatch(session, fromSequence, reason);
	}

	string Acknowledge(
		int playerId,
		int protocolVersion,
		int epoch,
		string snapshotId,
		int sequence,
		string registryHash)
	{
		HST_MarkerProjectionSessionState session = m_mSessions.Get(playerId);
		if (!session || !session.m_bReady)
			return "rejected: projection session not ready";
		if (protocolVersion != HST_MarkerProjectionCodec.PROTOCOL_VERSION)
		{
			return RejectAcknowledgement(session, "rejected: protocol mismatch", "acknowledgement protocol mismatch");
		}
		if (snapshotId.Length() > MAX_CLIENT_HASH_CHARACTERS || registryHash.Length() > MAX_CLIENT_HASH_CHARACTERS)
			return RejectAcknowledgement(session, "rejected: acknowledgement field exceeds protocol limit", "acknowledgement field exceeds protocol limit");
		if (epoch != m_iEpoch || epoch != session.m_iEpoch)
		{
			return RejectAcknowledgement(session, "rejected: projection epoch mismatch", "acknowledgement epoch mismatch");
		}
		if (sequence < session.m_iAcknowledgedSequence || sequence > m_iCurrentSequence || sequence > session.m_iLastSentSequence)
		{
			return RejectAcknowledgement(session, "rejected: acknowledgement sequence outside sent range", "acknowledgement sequence outside sent range");
		}

		if (session.m_bAwaitingSnapshotAcknowledge)
		{
			if (snapshotId != session.m_sSnapshotId || sequence != session.m_iSnapshotWatermark || registryHash != session.m_sExpectedAcknowledgeHash)
			{
				return RejectAcknowledgement(session, "rejected: snapshot acknowledgement mismatch", "snapshot acknowledgement mismatch");
			}
			session.m_bAwaitingSnapshotAcknowledge = false;
			session.m_bAwaitingDeltaAcknowledge = false;
			session.m_sSnapshotId = "";
		}
		else if (session.m_bAwaitingDeltaAcknowledge)
		{
			if (!snapshotId.IsEmpty())
			{
				return RejectAcknowledgement(session, "rejected: unexpected snapshot identity", "unexpected snapshot identity on delta acknowledgement");
			}
			if (sequence != session.m_iLastSentSequence || registryHash != session.m_sExpectedAcknowledgeHash)
			{
				return RejectAcknowledgement(session, "rejected: delta registry hash mismatch", "delta acknowledgement mismatch");
			}
			session.m_bAwaitingDeltaAcknowledge = false;
		}
		else
		{
			if (!snapshotId.IsEmpty()
				|| sequence != session.m_iAcknowledgedSequence
				|| (!registryHash.IsEmpty() && registryHash != m_sCurrentRegistryHash))
			{
				return RejectAcknowledgement(session, "rejected: acknowledgement has no matching in-flight dispatch", "acknowledgement without matching dispatch");
			}
		}

		session.m_iAcknowledgedSequence = sequence;
		session.m_bForceSnapshot = false;
		session.m_iInFlightDispatchTick = 0;
		session.m_sLastReason = "acknowledged";
		PruneJournal();
		return string.Format("accepted: epoch %1 sequence %2", epoch, sequence);
	}

	void RequestResync(int playerId, string reason)
	{
		if (!m_bInitialized || playerId <= 0)
			return;

		HST_MarkerProjectionSessionState session = ResolveSession(playerId);
		int nowTick = System.GetTickCount();
		if (IsRequestRateLimited(session.m_iLastResyncRequestTick, nowTick))
			return;
		bool recoveringDelta = session.m_bAwaitingDeltaAcknowledge;
		if (!recoveringDelta && IsInFlightRestartRateLimited(session, nowTick))
			return;
		session.m_iLastResyncRequestTick = nowTick;
		session.m_bReady = true;
		ForceSessionToSnapshot(session, SanitizeClientText(reason, MAX_CLIENT_REASON_CHARACTERS));
		session.m_bAllowImmediateSnapshotRestart = recoveringDelta;
	}

	void RemovePlayer(int playerId)
	{
		m_mSessions.Remove(playerId);
		PruneJournal();
	}

	void ForceAllReadySessionsToSnapshot(string reason)
	{
		if (!m_bInitialized)
			return;
		ForceAllSessionsToSnapshot(reason);
	}

	int GetEpoch()
	{
		return m_iEpoch;
	}

	int GetCurrentSequence()
	{
		return m_iCurrentSequence;
	}

	int GetJournalCount()
	{
		return m_aJournal.Count();
	}

	int GetSessionCount()
	{
		return m_mSessions.Count();
	}

	string GetCurrentRegistryHash()
	{
		return m_sCurrentRegistryHash;
	}

	void CopyCurrentRecords(notnull map<string, ref HST_MapMarkerState> target)
	{
		target.Clear();
		foreach (string id, HST_MapMarkerState marker : m_mCurrentRecords)
			target.Set(id, HST_MarkerProjectionCodec.CopyMarker(marker));
	}

	string BuildReport()
	{
		int readySessions;
		int awaitingSnapshots;
		int lowestAck = m_iCurrentSequence;
		foreach (int playerId, HST_MarkerProjectionSessionState session : m_mSessions)
		{
			if (!session || !session.m_bReady)
				continue;
			readySessions++;
			lowestAck = Math.Min(lowestAck, session.m_iAcknowledgedSequence);
			if (session.m_bAwaitingSnapshotAcknowledge)
				awaitingSnapshots++;
		}

		string report = string.Format(
			"Partisan marker projection | protocol %1 | epoch %2 | watermark %3 | records %4 | live hash %5",
			HST_MarkerProjectionCodec.PROTOCOL_VERSION,
			m_iEpoch,
			m_iCurrentSequence,
			m_mCurrentRecords.Count(),
			m_sCurrentRegistryHash);
		report = report + string.Format(
			" | journal %1 | sessions %2/%3 | awaiting snapshot %4 | lowest ack %5 | %6",
			m_aJournal.Count(),
			readySessions,
			m_mSessions.Count(),
			awaitingSnapshots,
			lowestAck,
			m_sLastSynchronizationReason);
		return report;
	}

	protected HST_MarkerProjectionDispatch BuildSnapshotDispatch(HST_MarkerProjectionSessionState session, string reason)
	{
		HST_MarkerProjectionDispatch dispatch = new HST_MarkerProjectionDispatch();
		if (!session)
			return dispatch;
		int nowTick = System.GetTickCount();
		if (!session.m_bAllowImmediateSnapshotRestart && IsSnapshotRestartRateLimited(session, nowTick))
		{
			session.m_bForceSnapshot = true;
			dispatch.m_sReason = reason + " | snapshot restart rate limited";
			return dispatch;
		}
		session.m_bAllowImmediateSnapshotRestart = false;

		m_iSnapshotSequence++;
		string snapshotId = string.Format("marker_snapshot_%1_%2_%3", m_iEpoch, m_iCurrentSequence, m_iSnapshotSequence);
		bool snapshotBuilt = HST_MarkerProjectionCodec.BuildSnapshotPackets(
			m_mCurrentRecords,
			m_iEpoch,
			snapshotId,
			m_iCurrentSequence,
			m_sCurrentRegistryHash,
			dispatch.m_aPackets);
		if (!snapshotBuilt || dispatch.m_aPackets.IsEmpty())
		{
			dispatch.m_aPackets.Clear();
			dispatch.m_sReason = reason + " | snapshot payload exceeds protocol bounds";
			session.m_bForceSnapshot = true;
			session.m_bAwaitingSnapshotAcknowledge = false;
			session.m_bAwaitingDeltaAcknowledge = false;
			session.m_sLastReason = dispatch.m_sReason;
			return dispatch;
		}

		dispatch.m_bSnapshot = true;
		dispatch.m_iEpoch = m_iEpoch;
		dispatch.m_iFromSequence = 0;
		dispatch.m_iToSequence = m_iCurrentSequence;
		dispatch.m_sSnapshotId = snapshotId;
		dispatch.m_sRegistryHash = m_sCurrentRegistryHash;
		dispatch.m_sReason = reason;

		session.m_iEpoch = m_iEpoch;
		session.m_bAwaitingSnapshotAcknowledge = true;
		session.m_bAwaitingDeltaAcknowledge = false;
		session.m_bForceSnapshot = false;
		session.m_iSnapshotWatermark = m_iCurrentSequence;
		session.m_iLastSentSequence = m_iCurrentSequence;
		session.m_sSnapshotId = snapshotId;
		session.m_sExpectedAcknowledgeHash = m_sCurrentRegistryHash;
		session.m_iLastSnapshotDispatchTick = nowTick;
		session.m_iInFlightDispatchTick = nowTick;
		session.m_sLastReason = reason;
		return dispatch;
	}

	protected HST_MarkerProjectionDispatch BuildDeltaDispatch(HST_MarkerProjectionSessionState session, int fromSequence, string reason)
	{
		HST_MarkerProjectionDispatch dispatch = new HST_MarkerProjectionDispatch();
		if (!session || fromSequence > m_iCurrentSequence)
			return dispatch;

		array<ref HST_MapMarkerState> events = {};
		for (int sequence = fromSequence; sequence <= m_iCurrentSequence; sequence++)
		{
			HST_MapMarkerState eventRecord = FindJournalRecord(sequence);
			if (!eventRecord)
				return BuildSnapshotDispatch(session, reason + " | delta gap");
			events.Insert(eventRecord);
		}

		bool deltaBuilt = HST_MarkerProjectionCodec.BuildDeltaPackets(events, m_iEpoch, m_sCurrentRegistryHash, dispatch.m_aPackets);
		if (!deltaBuilt || dispatch.m_aPackets.IsEmpty())
		{
			ForceSessionToSnapshot(session, reason + " | delta payload exceeds protocol bounds");
			return BuildSnapshotDispatch(session, reason + " | delta fallback snapshot");
		}
		dispatch.m_iEpoch = m_iEpoch;
		dispatch.m_iFromSequence = fromSequence;
		dispatch.m_iToSequence = m_iCurrentSequence;
		dispatch.m_sRegistryHash = m_sCurrentRegistryHash;
		dispatch.m_sReason = reason;
		session.m_iLastSentSequence = m_iCurrentSequence;
		session.m_bAwaitingDeltaAcknowledge = true;
		session.m_iInFlightDispatchTick = System.GetTickCount();
		session.m_sExpectedAcknowledgeHash = m_sCurrentRegistryHash;
		session.m_sLastReason = reason;
		return dispatch;
	}

	protected HST_MarkerProjectionSessionState ResolveSession(int playerId)
	{
		HST_MarkerProjectionSessionState session = m_mSessions.Get(playerId);
		if (session)
			return session;

		session = new HST_MarkerProjectionSessionState();
		session.m_iPlayerId = playerId;
		session.m_iEpoch = m_iEpoch;
		m_mSessions.Set(playerId, session);
		return session;
	}

	protected void CaptureCurrentRecords(HST_CampaignState state)
	{
		m_mCurrentRecords.Clear();
		if (state)
		{
			foreach (HST_MapMarkerState marker : state.m_aMapMarkers)
			{
				if (!marker || marker.m_sMarkerId.IsEmpty())
					continue;
				m_mCurrentRecords.Set(marker.m_sMarkerId, HST_MarkerProjectionCodec.CopyMarker(marker));
			}
		}
		m_sCurrentRegistryHash = HST_MarkerProjectionCodec.BuildLiveRegistryHash(m_mCurrentRecords);
	}

	protected HST_MapMarkerState FindRecordBySequence(HST_CampaignState state, int sequence)
	{
		if (!state || sequence <= 0)
			return null;
		foreach (HST_MapMarkerState marker : state.m_aMapMarkers)
		{
			if (marker && marker.m_iStreamSequence == sequence)
				return marker;
		}
		return null;
	}

	protected bool StateMatchesExpectedProjection(
		HST_CampaignState state,
		notnull array<ref HST_MapMarkerState> newEvents)
	{
		if (!state)
			return false;

		map<string, ref HST_MapMarkerState> expected = new map<string, ref HST_MapMarkerState>();
		foreach (string currentId, HST_MapMarkerState currentMarker : m_mCurrentRecords)
			expected.Set(currentId, HST_MarkerProjectionCodec.CopyMarker(currentMarker));
		foreach (HST_MapMarkerState eventMarker : newEvents)
		{
			if (!eventMarker || eventMarker.m_sMarkerId.IsEmpty())
				return false;
			expected.Set(eventMarker.m_sMarkerId, HST_MarkerProjectionCodec.CopyMarker(eventMarker));
		}

		map<string, ref HST_MapMarkerState> actual = new map<string, ref HST_MapMarkerState>();
		foreach (HST_MapMarkerState stateMarker : state.m_aMapMarkers)
		{
			if (!stateMarker || stateMarker.m_sMarkerId.IsEmpty() || actual.Contains(stateMarker.m_sMarkerId))
				return false;
			actual.Set(stateMarker.m_sMarkerId, stateMarker);
		}
		if (actual.Count() != expected.Count())
			return false;

		foreach (string markerId, HST_MapMarkerState expectedMarker : expected)
		{
			HST_MapMarkerState actualMarker = actual.Get(markerId);
			if (!actualMarker || !HST_MarkerProjectionCodec.TransportEquals(expectedMarker, actualMarker))
				return false;
		}
		return true;
	}

	protected HST_MapMarkerState FindJournalRecord(int sequence)
	{
		foreach (HST_MapMarkerState marker : m_aJournal)
		{
			if (marker && marker.m_iStreamSequence == sequence)
				return marker;
		}
		return null;
	}

	protected bool CanReplayFrom(int fromSequence)
	{
		if (fromSequence > m_iCurrentSequence)
			return true;
		if (fromSequence <= 0)
			return false;
		for (int sequence = fromSequence; sequence <= m_iCurrentSequence; sequence++)
		{
			if (!FindJournalRecord(sequence))
				return false;
		}
		return true;
	}

	protected void ForceAllSessionsToSnapshot(string reason)
	{
		foreach (int playerId, HST_MarkerProjectionSessionState session : m_mSessions)
		{
			if (!session)
				continue;
			ForceSessionToSnapshot(session, reason);
			session.m_iLastSnapshotDispatchTick = 0;
		}
	}

	protected void ForceSessionToSnapshot(HST_MarkerProjectionSessionState session, string reason)
	{
		if (!session)
			return;
		session.m_bForceSnapshot = true;
		session.m_bAwaitingSnapshotAcknowledge = false;
		session.m_bAwaitingDeltaAcknowledge = false;
		session.m_bAllowImmediateSnapshotRestart = false;
		session.m_iEpoch = m_iEpoch;
		session.m_iAcknowledgedSequence = 0;
		session.m_iLastSentSequence = 0;
		session.m_iSnapshotWatermark = 0;
		session.m_iInFlightDispatchTick = 0;
		session.m_sSnapshotId = "";
		session.m_sExpectedAcknowledgeHash = "";
		session.m_sLastReason = reason;
	}

	protected string RejectAcknowledgement(
		HST_MarkerProjectionSessionState session,
		string response,
		string reason)
	{
		if (!session)
			return response;
		int nowTick = System.GetTickCount();
		if (IsRequestRateLimited(session.m_iLastRejectedAcknowledgeTick, nowTick))
			return "rejected: acknowledgement recovery rate limited";
		session.m_iLastRejectedAcknowledgeTick = nowTick;
		bool recoveringDelta = session.m_bAwaitingDeltaAcknowledge;
		ForceSessionToSnapshot(session, reason);
		session.m_bAllowImmediateSnapshotRestart = recoveringDelta;
		return response;
	}

	protected bool IsRequestRateLimited(int previousTick, int nowTick)
	{
		return previousTick > 0 && nowTick >= previousTick && nowTick - previousTick < REQUEST_COOLDOWN_MS;
	}

	protected bool IsSnapshotRestartRateLimited(HST_MarkerProjectionSessionState session, int nowTick)
	{
		return session
			&& session.m_iLastSnapshotDispatchTick > 0
			&& nowTick >= session.m_iLastSnapshotDispatchTick
			&& nowTick - session.m_iLastSnapshotDispatchTick < SNAPSHOT_RESTART_COOLDOWN_MS;
	}

	protected bool IsInFlightRestartRateLimited(HST_MarkerProjectionSessionState session, int nowTick)
	{
		return session
			&& session.m_iInFlightDispatchTick > 0
			&& nowTick >= session.m_iInFlightDispatchTick
			&& nowTick - session.m_iInFlightDispatchTick < SNAPSHOT_RESTART_COOLDOWN_MS;
	}

	protected string SanitizeClientText(string value, int maxCharacters)
	{
		value.Replace("\r", " ");
		value.Replace("\n", " ");
		if (value.Length() > maxCharacters)
			return value.Substring(0, maxCharacters);
		return value;
	}

	protected void PruneJournal()
	{
		int minimumAck = m_iCurrentSequence;
		bool foundReady;
		foreach (int playerId, HST_MarkerProjectionSessionState session : m_mSessions)
		{
			if (!session || !session.m_bReady || session.m_bAwaitingSnapshotAcknowledge)
				continue;
			foundReady = true;
			minimumAck = Math.Min(minimumAck, session.m_iAcknowledgedSequence);
		}

		if (foundReady)
		{
			while (!m_aJournal.IsEmpty() && m_aJournal[0] && m_aJournal[0].m_iStreamSequence <= minimumAck)
				m_aJournal.Remove(0);
		}
		while (m_aJournal.Count() > MAX_JOURNAL_EVENTS)
			m_aJournal.Remove(0);
	}
}
