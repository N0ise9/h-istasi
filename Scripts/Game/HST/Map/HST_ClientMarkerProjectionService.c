class HST_ClientMarkerProjectionRegistry
{
	protected ref map<string, ref HST_MapMarkerState> m_mRecords = new map<string, ref HST_MapMarkerState>();
	protected ref map<string, ref HST_MapMarkerState> m_mSnapshotStaging = new map<string, ref HST_MapMarkerState>();
	protected ref array<bool> m_aSnapshotChunksReceived = {};
	protected bool m_bReady;
	protected int m_iEpoch;
	protected int m_iWatermark;
	protected string m_sRegistryHash = "0";
	protected string m_sSnapshotId;
	protected int m_iSnapshotEpoch;
	protected int m_iSnapshotWatermark;
	protected int m_iSnapshotChunkCount;
	protected int m_iSnapshotExpectedRecords;
	protected string m_sSnapshotExpectedHash;
	protected string m_sLastStatus = "not ready";

	HST_MarkerProjectionApplyResult ApplyPacket(string payload)
	{
		HST_MarkerProjectionPacket packet = HST_MarkerProjectionCodec.DecodePacket(payload);
		if (!packet)
			return Reject("malformed marker projection packet", true);
		if (packet.m_sKind == HST_MarkerProjectionCodec.SNAPSHOT_HEADER)
			return ApplySnapshotChunk(packet);
		if (packet.m_sKind == HST_MarkerProjectionCodec.DELTA_HEADER)
			return ApplyDelta(packet);
		return Reject("unsupported marker projection packet", true);
	}

	bool IsReady()
	{
		return m_bReady;
	}

	int GetEpoch()
	{
		return m_iEpoch;
	}

	int GetWatermark()
	{
		return m_iWatermark;
	}

	int GetRecordCount()
	{
		return m_mRecords.Count();
	}

	int GetLiveRecordCount()
	{
		int count;
		foreach (string id, HST_MapMarkerState marker : m_mRecords)
		{
			if (marker && !marker.m_bTombstone)
				count++;
		}
		return count;
	}

	string GetRegistryHash()
	{
		return m_sRegistryHash;
	}

	string GetLastStatus()
	{
		return m_sLastStatus;
	}

	HST_MapMarkerState FindRecord(string markerId)
	{
		return m_mRecords.Get(markerId);
	}

	void CopyRecords(notnull map<string, ref HST_MapMarkerState> target, bool includeTombstones = true)
	{
		target.Clear();
		foreach (string id, HST_MapMarkerState marker : m_mRecords)
		{
			if (!marker || (!includeTombstones && marker.m_bTombstone))
				continue;
			target.Set(id, HST_MarkerProjectionCodec.CopyMarker(marker));
		}
	}

	void Reset(string reason)
	{
		m_mRecords.Clear();
		ResetSnapshotStaging();
		m_bReady = false;
		m_iEpoch = 0;
		m_iWatermark = 0;
		m_sRegistryHash = "0";
		m_sLastStatus = reason;
	}

	void RequireSnapshot(string reason)
	{
		ResetSnapshotStaging();
		m_bReady = false;
		m_sLastStatus = reason;
	}

	protected HST_MarkerProjectionApplyResult ApplySnapshotChunk(HST_MarkerProjectionPacket packet)
	{
		if (m_bReady && packet.m_iEpoch < m_iEpoch)
			return Reject("stale snapshot epoch", true);
		if (m_bReady && packet.m_iEpoch == m_iEpoch && packet.m_iWatermark < m_iWatermark)
			return Reject("stale snapshot watermark", true);

		if (packet.m_sSnapshotId != m_sSnapshotId)
		{
			ResetSnapshotStaging();
			m_sSnapshotId = packet.m_sSnapshotId;
			m_iSnapshotEpoch = packet.m_iEpoch;
			m_iSnapshotWatermark = packet.m_iWatermark;
			m_iSnapshotChunkCount = packet.m_iChunkCount;
			m_iSnapshotExpectedRecords = packet.m_iTotalRecordCount;
			m_sSnapshotExpectedHash = packet.m_sRegistryHash;
			m_aSnapshotChunksReceived.Resize(packet.m_iChunkCount);
		}

		if (packet.m_iEpoch != m_iSnapshotEpoch
			|| packet.m_iWatermark != m_iSnapshotWatermark
			|| packet.m_iChunkCount != m_iSnapshotChunkCount
			|| packet.m_iTotalRecordCount != m_iSnapshotExpectedRecords
			|| packet.m_sRegistryHash != m_sSnapshotExpectedHash)
			return Reject("snapshot chunk metadata mismatch", true);

		if (m_aSnapshotChunksReceived[packet.m_iChunkIndex])
		{
			HST_MarkerProjectionApplyResult duplicate = Accept("duplicate snapshot chunk ignored");
			return duplicate;
		}

		foreach (HST_MapMarkerState marker : packet.m_aRecords)
		{
			if (!marker || marker.m_iStreamSequence > packet.m_iWatermark)
				return Reject("snapshot record exceeds watermark", true);
			HST_MapMarkerState existing = m_mSnapshotStaging.Get(marker.m_sMarkerId);
			if (existing && !HST_MarkerProjectionCodec.TransportEquals(existing, marker))
				return Reject("snapshot contains conflicting duplicate marker id", true);
			m_mSnapshotStaging.Set(marker.m_sMarkerId, HST_MarkerProjectionCodec.CopyMarker(marker));
		}
		m_aSnapshotChunksReceived[packet.m_iChunkIndex] = true;

		for (int i; i < m_aSnapshotChunksReceived.Count(); i++)
		{
			if (!m_aSnapshotChunksReceived[i])
				return Accept("snapshot chunk staged");
		}

		if (m_mSnapshotStaging.Count() != m_iSnapshotExpectedRecords)
			return Reject("snapshot record count mismatch", true);
		string stagedHash = HST_MarkerProjectionCodec.BuildLiveRegistryHash(m_mSnapshotStaging);
		if (stagedHash != m_sSnapshotExpectedHash)
			return Reject("snapshot registry hash mismatch", true);

		m_mRecords.Clear();
		foreach (string id, HST_MapMarkerState stagedMarker : m_mSnapshotStaging)
			m_mRecords.Set(id, HST_MarkerProjectionCodec.CopyMarker(stagedMarker));
		m_bReady = true;
		m_iEpoch = m_iSnapshotEpoch;
		m_iWatermark = m_iSnapshotWatermark;
		m_sRegistryHash = stagedHash;

		HST_MarkerProjectionApplyResult result = Accept("snapshot committed");
		result.m_bSnapshotCommitted = true;
		result.m_bRegistryChanged = true;
		result.m_bNeedsAcknowledge = true;
		result.m_iEpoch = m_iEpoch;
		result.m_iAcknowledgeSequence = m_iWatermark;
		result.m_sSnapshotId = m_sSnapshotId;
		result.m_sRegistryHash = m_sRegistryHash;
		ResetSnapshotStaging();
		return result;
	}

	protected HST_MarkerProjectionApplyResult ApplyDelta(HST_MarkerProjectionPacket packet)
	{
		if (!m_bReady)
			return Reject("delta received before snapshot readiness", true);
		if (packet.m_iEpoch != m_iEpoch)
			return Reject("delta epoch mismatch", true);
		if (packet.m_iToSequence <= m_iWatermark)
		{
			HST_MarkerProjectionApplyResult duplicate = Accept("duplicate delta ignored");
			duplicate.m_bNeedsAcknowledge = !packet.m_sRegistryHash.IsEmpty();
			duplicate.m_iEpoch = m_iEpoch;
			duplicate.m_iAcknowledgeSequence = m_iWatermark;
			if (!packet.m_sRegistryHash.IsEmpty())
				duplicate.m_sRegistryHash = m_sRegistryHash;
			return duplicate;
		}
		if (packet.m_iFromSequence != m_iWatermark + 1)
			return Reject(string.Format("delta sequence gap: expected %1, received %2", m_iWatermark + 1, packet.m_iFromSequence), true);
		if (packet.m_aRecords.Count() != packet.m_iToSequence - packet.m_iFromSequence + 1)
			return Reject("delta event count is not contiguous", true);

		map<string, ref HST_MapMarkerState> working = new map<string, ref HST_MapMarkerState>();
		foreach (string currentId, HST_MapMarkerState currentMarker : m_mRecords)
			working.Set(currentId, HST_MarkerProjectionCodec.CopyMarker(currentMarker));

		int expectedSequence = packet.m_iFromSequence;
		foreach (HST_MapMarkerState marker : packet.m_aRecords)
		{
			if (!marker || marker.m_iStreamSequence != expectedSequence)
				return Reject("delta record sequence mismatch", true);
			expectedSequence++;

			HST_MapMarkerState existing = working.Get(marker.m_sMarkerId);
			if (!existing)
			{
				if (marker.m_iRevision != 1)
					return Reject("new delta marker does not begin at revision one", true);
			}
			else
			{
				if (marker.m_iRevision != existing.m_iRevision + 1)
					return Reject("delta marker revision is not the next revision", true);
			}
			working.Set(marker.m_sMarkerId, HST_MarkerProjectionCodec.CopyMarker(marker));
		}

		string workingHash = HST_MarkerProjectionCodec.BuildLiveRegistryHash(working);
		if (!packet.m_sRegistryHash.IsEmpty() && workingHash != packet.m_sRegistryHash)
			return Reject("delta registry hash mismatch", true);

		m_mRecords.Clear();
		foreach (string id, HST_MapMarkerState updatedMarker : working)
			m_mRecords.Set(id, HST_MarkerProjectionCodec.CopyMarker(updatedMarker));
		m_iWatermark = packet.m_iToSequence;
		m_sRegistryHash = workingHash;

		HST_MarkerProjectionApplyResult result = Accept("delta committed");
		result.m_bRegistryChanged = true;
		result.m_bNeedsAcknowledge = !packet.m_sRegistryHash.IsEmpty();
		result.m_iEpoch = m_iEpoch;
		result.m_iAcknowledgeSequence = m_iWatermark;
		if (!packet.m_sRegistryHash.IsEmpty())
			result.m_sRegistryHash = m_sRegistryHash;
		return result;
	}

	protected HST_MarkerProjectionApplyResult Accept(string reason)
	{
		HST_MarkerProjectionApplyResult result = new HST_MarkerProjectionApplyResult();
		result.m_bAccepted = true;
		result.m_iEpoch = m_iEpoch;
		result.m_iAcknowledgeSequence = m_iWatermark;
		result.m_sReason = reason;
		m_sLastStatus = reason;
		return result;
	}

	protected HST_MarkerProjectionApplyResult Reject(string reason, bool needsResync)
	{
		HST_MarkerProjectionApplyResult result = new HST_MarkerProjectionApplyResult();
		result.m_bNeedsResync = needsResync;
		result.m_iEpoch = m_iEpoch;
		result.m_iAcknowledgeSequence = m_iWatermark;
		result.m_sReason = reason;
		m_sLastStatus = reason;
		return result;
	}

	protected void ResetSnapshotStaging(bool clearIdentity = true)
	{
		m_mSnapshotStaging.Clear();
		m_aSnapshotChunksReceived.Clear();
		m_iSnapshotEpoch = 0;
		m_iSnapshotWatermark = 0;
		m_iSnapshotChunkCount = 0;
		m_iSnapshotExpectedRecords = 0;
		m_sSnapshotExpectedHash = "";
		if (clearIdentity)
			m_sSnapshotId = "";
	}
}

class HST_ClientMarkerProjectionService
{
	protected ref HST_ClientMarkerProjectionRegistry m_Registry = new HST_ClientMarkerProjectionRegistry();
	protected ref HST_CampaignMapMarkerDirector m_Director = new HST_CampaignMapMarkerDirector();
	protected ref HST_NativeMapMarkerReconciler m_Reconciler = new HST_NativeMapMarkerReconciler();
	protected ref map<string, ref HST_MapMarkerRecord> m_mDesired = new map<string, ref HST_MapMarkerRecord>();
	protected ref map<string, MapItem> m_mAuthoredZoneMapItemByZoneId = new map<string, MapItem>();
	protected ref map<string, bool> m_mAuthoredZoneLookupAttempted = new map<string, bool>();
	protected ref map<string, bool> m_mSuppressedAuthoredZoneIds = new map<string, bool>();
	protected ref map<string, bool> m_mAuthoredZonePriorVisibility = new map<string, bool>();
	protected int m_iLastAuthoredZoneDescriptorsHidden;
	protected int m_iLastAuthoredZoneDescriptorsMissing;

	HST_MarkerProjectionApplyResult ReceivePacket(string payload)
	{
		HST_MarkerProjectionApplyResult result = m_Registry.ApplyPacket(payload);
		if (result
			&& result.m_bAccepted
			&& result.m_bRegistryChanged
			&& (result.m_bSnapshotCommitted || result.m_bNeedsAcknowledge))
			ReconcileNativeMarkers();
		return result;
	}

	bool ReconcileNativeMarkers()
	{
		if (!m_Registry.IsReady())
			return false;

		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerManager)
			return false;

		HST_CampaignState projectionState = new HST_CampaignState();
		map<string, ref HST_MapMarkerState> records = new map<string, ref HST_MapMarkerState>();
		m_Registry.CopyRecords(records, false);
		array<string> markerIds = {};
		foreach (string id, HST_MapMarkerState marker : records)
			markerIds.Insert(id);
		markerIds.Sort();
		foreach (string markerId : markerIds)
			projectionState.m_aMapMarkers.Insert(records.Get(markerId));

		m_Director.BuildDesiredMarkers(projectionState, null, m_mDesired);
		foreach (string markerId, HST_MapMarkerRecord record : m_mDesired)
		{
			if (!record)
				continue;
			record.m_bLocalOnly = true;
			record.m_bServerMarker = false;
		}

		m_Reconciler.Reconcile(markerManager, m_mDesired);
		bool authoredDescriptorsReady = ReconcileAuthoredZoneDescriptors(markerManager, records);
		return m_Reconciler.GetLastResult().m_iFailed == 0 && authoredDescriptorsReady;
	}

	protected bool ReconcileAuthoredZoneDescriptors(
		SCR_MapMarkerManagerComponent markerManager,
		notnull map<string, ref HST_MapMarkerState> liveRecords)
	{
		m_iLastAuthoredZoneDescriptorsHidden = 0;
		m_iLastAuthoredZoneDescriptorsMissing = 0;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		array<string> suppressedZoneIds = {};
		foreach (string suppressedZoneId, bool suppressed : m_mSuppressedAuthoredZoneIds)
			suppressedZoneIds.Insert(suppressedZoneId);
		foreach (string suppressedZoneId : suppressedZoneIds)
		{
			string suppressedMarkerId = "hst_zone_" + suppressedZoneId;
			if (m_mDesired.Contains(suppressedMarkerId)
				&& m_Reconciler.IsDomainIdLive(markerManager, suppressedMarkerId))
				continue;
			RestoreAuthoredZoneDescriptor(suppressedZoneId);
		}

		foreach (string markerId, HST_MapMarkerState marker : liveRecords)
		{
			if (!marker || marker.m_bTombstone || markerId.IndexOf("hst_zone_") != 0 || marker.m_sLinkedId.IsEmpty())
				continue;
			if (!m_mDesired.Contains(markerId) || !m_Reconciler.IsDomainIdLive(markerManager, markerId))
				continue;

			MapItem mapItem = ResolveAuthoredZoneMapItem(marker.m_sLinkedId, world);
			if (!mapItem)
			{
				m_iLastAuthoredZoneDescriptorsMissing++;
				continue;
			}

			if (!m_mSuppressedAuthoredZoneIds.Contains(marker.m_sLinkedId))
				m_mAuthoredZonePriorVisibility.Set(marker.m_sLinkedId, mapItem.IsVisible());
			if (mapItem.IsVisible())
				mapItem.SetVisible(false);
			m_mSuppressedAuthoredZoneIds.Set(marker.m_sLinkedId, true);
		}
		m_iLastAuthoredZoneDescriptorsHidden = m_mSuppressedAuthoredZoneIds.Count();
		return m_iLastAuthoredZoneDescriptorsMissing == 0;
	}

	protected MapItem ResolveAuthoredZoneMapItem(string zoneId, BaseWorld world)
	{
		MapItem cached = m_mAuthoredZoneMapItemByZoneId.Get(zoneId);
		if (cached)
			return cached;
		if (!world || zoneId.IsEmpty() || m_mAuthoredZoneLookupAttempted.Contains(zoneId))
			return null;

		m_mAuthoredZoneLookupAttempted.Set(zoneId, true);
		string entityName = "HST_ConflictMapMarker_" + zoneId;
		IEntity markerEntity = world.FindEntityByName(entityName);
		if (!markerEntity || markerEntity.GetName() != entityName)
			return null;
		SCR_CampaignMilitaryBaseMapDescriptorComponent descriptor = SCR_CampaignMilitaryBaseMapDescriptorComponent.Cast(markerEntity.FindComponent(SCR_CampaignMilitaryBaseMapDescriptorComponent));
		if (!descriptor)
			return null;
		MapItem mapItem = descriptor.Item();
		if (mapItem)
			m_mAuthoredZoneMapItemByZoneId.Set(zoneId, mapItem);
		return mapItem;
	}

	protected void RestoreAuthoredZoneDescriptor(string zoneId)
	{
		MapItem mapItem = m_mAuthoredZoneMapItemByZoneId.Get(zoneId);
		if (mapItem && m_mAuthoredZonePriorVisibility.Contains(zoneId))
			mapItem.SetVisible(m_mAuthoredZonePriorVisibility.Get(zoneId));
		m_mSuppressedAuthoredZoneIds.Remove(zoneId);
		m_mAuthoredZonePriorVisibility.Remove(zoneId);
	}

	protected void RestoreAllAuthoredZoneDescriptors()
	{
		array<string> suppressedZoneIds = {};
		foreach (string zoneId, bool suppressed : m_mSuppressedAuthoredZoneIds)
			suppressedZoneIds.Insert(zoneId);
		foreach (string zoneId : suppressedZoneIds)
			RestoreAuthoredZoneDescriptor(zoneId);
		m_iLastAuthoredZoneDescriptorsHidden = 0;
	}

	void RetryMissingAuthoredZoneDescriptorBindings()
	{
		array<string> missingZoneIds = {};
		foreach (string zoneId, bool attempted : m_mAuthoredZoneLookupAttempted)
		{
			if (!m_mAuthoredZoneMapItemByZoneId.Contains(zoneId))
				missingZoneIds.Insert(zoneId);
		}
		foreach (string zoneId : missingZoneIds)
			m_mAuthoredZoneLookupAttempted.Remove(zoneId);
	}

	void Clear()
	{
		RestoreAllAuthoredZoneDescriptors();
		m_Reconciler.Clear();
		m_mDesired.Clear();
		m_Registry.Reset("client projection cleared");
	}

	void RequireSnapshot(string reason)
	{
		m_Registry.RequireSnapshot(reason);
	}

	HST_ClientMarkerProjectionRegistry GetRegistry()
	{
		return m_Registry;
	}

	string BuildReport()
	{
		string report = string.Format(
			"h-istasi client marker projection | ready %1 | epoch %2 | watermark %3 | records %4 live %5 | hash %6 | status %7",
			m_Registry.IsReady(),
			m_Registry.GetEpoch(),
			m_Registry.GetWatermark(),
			m_Registry.GetRecordCount(),
			m_Registry.GetLiveRecordCount(),
			m_Registry.GetRegistryHash(),
			m_Registry.GetLastStatus());
		report = report + string.Format(
			" | authored zones hidden/missing %1/%2 | native %3",
			m_iLastAuthoredZoneDescriptorsHidden,
			m_iLastAuthoredZoneDescriptorsMissing,
			m_Reconciler.BuildRuntimeReport());
		return report;
	}
}
