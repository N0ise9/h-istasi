// Schema 61 makes campaign markers a revisioned projection stream. Marker rows
// are derived views, so legacy or malformed projection rows are discarded and
// rebuilt from authoritative campaign aggregates instead of being guessed.
class HST_MarkerProjectionSaveValidationService
{
	static const int SCHEMA_VERSION = 61;
	static const string MIGRATION_EVENT_ID = "migration_schema61_marker_projection";
	static const string REPAIR_EVENT_ID = "normalization_schema61_marker_projection_rebuild";

	void Normalize(HST_CampaignSaveData saveData, int restoredSchemaVersion)
	{
		if (!saveData)
			return;

		saveData.m_iMarkerProjectionEpoch = Math.Max(1, saveData.m_iMarkerProjectionEpoch);
		saveData.m_iMarkerProjectionSequence = Math.Max(0, saveData.m_iMarkerProjectionSequence);
		if (restoredSchemaVersion < SCHEMA_VERSION)
		{
			int retiredRows = saveData.m_aMapMarkers.Count();
			saveData.m_aMapMarkers.Clear();
			saveData.m_iMarkerProjectionSequence = 0;
			RecordEvent(
				saveData,
				MIGRATION_EVENT_ID,
				"legacy_projection_retired",
				string.Format("restored schema %1; retired %2 derived marker rows so Schema 61 can rebuild exact revisions and stream order from campaign aggregates", restoredSchemaVersion, retiredRows));
			return;
		}

		string failure = ValidateCurrentRows(saveData);
		if (failure.IsEmpty())
			return;

		int invalidRows = saveData.m_aMapMarkers.Count();
		saveData.m_aMapMarkers.Clear();
		saveData.m_iMarkerProjectionEpoch++;
		saveData.m_iMarkerProjectionSequence = 0;
		RecordEvent(
			saveData,
			REPAIR_EVENT_ID,
			"derived_projection_rebuild_required",
			string.Format("discarded %1 malformed derived marker rows and advanced projection epoch: %2", invalidRows, failure));
	}

	protected string ValidateCurrentRows(HST_CampaignSaveData saveData)
	{
		map<string, bool> ids = new map<string, bool>();
		map<int, bool> sequences = new map<int, bool>();
		int highestSequence;
		foreach (HST_MapMarkerState marker : saveData.m_aMapMarkers)
		{
			if (!marker)
				return "null marker row";
			if (marker.m_sMarkerId.IsEmpty())
				return "marker row has no stable id";
			if (ids.Contains(marker.m_sMarkerId))
				return "duplicate marker id " + marker.m_sMarkerId;
			if (marker.m_iRevision <= 0 || marker.m_iStreamSequence <= 0)
				return "marker revision or stream sequence is non-positive for " + marker.m_sMarkerId;
			if (sequences.Contains(marker.m_iStreamSequence))
				return string.Format("duplicate marker stream sequence %1", marker.m_iStreamSequence);
			if (marker.m_bTombstone && marker.m_bVisible)
				return "visible tombstone " + marker.m_sMarkerId;

			ids.Set(marker.m_sMarkerId, true);
			sequences.Set(marker.m_iStreamSequence, true);
			highestSequence = Math.Max(highestSequence, marker.m_iStreamSequence);
		}

		if (saveData.m_iMarkerProjectionSequence < highestSequence)
			return string.Format("stream watermark %1 trails record sequence %2", saveData.m_iMarkerProjectionSequence, highestSequence);
		return "";
	}

	protected void RecordEvent(HST_CampaignSaveData saveData, string eventId, string transition, string reason)
	{
		if (!saveData || HasEvent(saveData, eventId))
			return;

		HST_CampaignEventState eventState = new HST_CampaignEventState();
		eventState.m_sEventId = eventId;
		eventState.m_sCategory = "migration";
		eventState.m_sAggregateType = "marker_projection";
		eventState.m_sAggregateId = "schema61";
		eventState.m_sTransition = transition;
		eventState.m_sReason = reason;
		eventState.m_iCreatedAtSecond = Math.Max(0, saveData.m_iElapsedSeconds);
		saveData.m_aCampaignEvents.Insert(eventState);
	}

	protected bool HasEvent(HST_CampaignSaveData saveData, string eventId)
	{
		foreach (HST_CampaignEventState eventState : saveData.m_aCampaignEvents)
		{
			if (eventState && eventState.m_sEventId == eventId)
				return true;
		}
		return false;
	}
}
