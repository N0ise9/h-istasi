// Engine-created transport proxy for the manually constructed campaign save
// snapshot. PersistentState instances are owned by the persistence system and
// must never be constructed directly by gameplay code.
class HST_CampaignPersistentState : PersistentState
{
	static const string ENVELOPE_MAGIC = "partisan_campaign_persistent_state_v1";
	static const int ENVELOPE_VERSION = 1;

	protected bool m_bLoadedRecordPresent;
	protected bool m_bLoadedRecordValid;
	protected string m_sValidationFailure;
	protected string m_sSnapshotFingerprint;
	protected ref HST_CampaignSaveData m_Snapshot;

	void SetSnapshotForSave(HST_CampaignSaveData snapshot)
	{
		m_Snapshot = snapshot;
		m_sSnapshotFingerprint = BuildSnapshotFingerprint(snapshot);
	}

	void SetLoadedSnapshot(
		HST_CampaignSaveData snapshot,
		string snapshotFingerprint)
	{
		m_bLoadedRecordPresent = true;
		m_bLoadedRecordValid = snapshot
			&& !snapshotFingerprint.IsEmpty()
			&& snapshotFingerprint == BuildSnapshotFingerprint(snapshot);
		m_sValidationFailure = "";
		m_Snapshot = snapshot;
		m_sSnapshotFingerprint = snapshotFingerprint;
		if (!m_bLoadedRecordValid)
			m_sValidationFailure = "native campaign payload fingerprint rejected";
	}

	void SetLoadedInvalid(string failure)
	{
		m_bLoadedRecordPresent = true;
		m_bLoadedRecordValid = false;
		m_sValidationFailure = failure;
		m_sSnapshotFingerprint = "";
		m_Snapshot = null;
	}

	bool HasLoadedRecord()
	{
		return m_bLoadedRecordPresent;
	}

	bool IsLoadedRecordValid()
	{
		return m_bLoadedRecordPresent && m_bLoadedRecordValid && m_Snapshot;
	}

	HST_CampaignSaveData GetSnapshot()
	{
		return m_Snapshot;
	}

	string GetSnapshotFingerprint()
	{
		return m_sSnapshotFingerprint;
	}

	string GetValidationFailure()
	{
		return m_sValidationFailure;
	}

	static string BuildSnapshotFingerprint(HST_CampaignSaveData snapshot)
	{
		if (!snapshot)
			return "";

		JsonSaveContext context = new JsonSaveContext();
		if (!context.WriteValue("", snapshot))
			return "";
		string payload = context.SaveToString();
		if (payload.IsEmpty())
			return "";
		return string.Format("%1:%2", payload.Length(), payload.Hash());
	}
}

class HST_CampaignPersistentStateSerializer : ScriptedStateSerializer
{
	override static typename GetTargetType()
	{
		return HST_CampaignPersistentState;
	}

	override ESerializeResult Serialize(
		notnull Managed instance,
		notnull SaveContext context)
	{
		HST_CampaignPersistentState persistentState
			= HST_CampaignPersistentState.Cast(instance);
		if (!persistentState)
			return ESerializeResult.ERROR;

		HST_CampaignSaveData snapshot = persistentState.GetSnapshot();
		if (!snapshot)
		{
			// This state is configured as required campaign authority. A save while
			// bootstrap has not armed its snapshot must fail, never commit a valid
			// looking save point with the campaign row silently omitted.
			return ESerializeResult.ERROR;
		}
		string fingerprint
			= HST_CampaignPersistentState.BuildSnapshotFingerprint(snapshot);
		if (fingerprint.IsEmpty())
			return ESerializeResult.ERROR;

		if (!context.WriteValue(
			"magic",
			HST_CampaignPersistentState.ENVELOPE_MAGIC)
			|| !context.WriteValue(
				"version",
				HST_CampaignPersistentState.ENVELOPE_VERSION)
			|| !context.WriteValue("snapshotPresent", true)
			|| !context.WriteValue("snapshotFingerprint", fingerprint)
			|| !context.WriteValue("snapshot", snapshot))
			return ESerializeResult.ERROR;

		return ESerializeResult.OK;
	}

	override bool Deserialize(
		notnull Managed instance,
		notnull LoadContext context)
	{
		HST_CampaignPersistentState persistentState
			= HST_CampaignPersistentState.Cast(instance);
		if (!persistentState)
			return false;

		string magic;
		int version;
		bool snapshotPresent;
		string storedFingerprint;
		HST_CampaignSaveData snapshot = new HST_CampaignSaveData();
		if (!context.ReadValue("magic", magic)
			|| !context.ReadValue("version", version)
			|| !context.ReadValue("snapshotPresent", snapshotPresent)
			|| !context.ReadValue("snapshotFingerprint", storedFingerprint)
			|| !context.ReadValue("snapshot", snapshot))
		{
			persistentState.SetLoadedInvalid(
				"native campaign envelope could not be read");
			return false;
		}
		if (magic != HST_CampaignPersistentState.ENVELOPE_MAGIC
			|| version != HST_CampaignPersistentState.ENVELOPE_VERSION
			|| !snapshotPresent || storedFingerprint.IsEmpty())
		{
			persistentState.SetLoadedInvalid(
				"native campaign envelope identity rejected");
			return false;
		}

		persistentState.SetLoadedSnapshot(snapshot, storedFingerprint);
		return persistentState.IsLoadedRecordValid();
	}
}
