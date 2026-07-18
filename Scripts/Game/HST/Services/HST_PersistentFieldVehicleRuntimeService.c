// Session-only live binding for durable field vehicles. Registered bindings are
// exact; the bounded same-prefab position lookup is recovery-only and fails
// closed when more than one durable row is plausible.
class HST_PersistentFieldVehicleNewCampaignResetPlan
{
	ref HST_CampaignState m_State;
	ref array<string> m_aRetainedRuntimeIds = {};
	ref array<string> m_aDeleteRuntimeIds = {};
	ref array<IEntity> m_aDeleteEntities = {};
	ref array<ref HST_RuntimeVehicleState> m_aDeleteRecords = {};
	bool m_bPrepared;
	bool m_bCommitted;
}

// One controlled-end fence pin owns the exact durable transform and process-
// local root identity admitted after the normal capture-preparation pass. Once
// any root mutation begins these pins are immutable for the rest of the process;
// retries reapply them instead of sampling a partially quiesced vehicle.
class HST_ControlledShutdownFieldVehiclePin
{
	string m_sRuntimeId;
	IEntity m_Entity;
	ref HST_RuntimeVehicleState m_Record;
	vector m_vDurablePosition;
	vector m_vDurableAngles;
}

class HST_PersistentFieldVehicleRuntimeService
{
	protected ref array<IEntity> m_aEntities = {};
	protected ref array<string> m_aRuntimeIds = {};
	protected bool m_bBindingFault;
	protected bool m_bControlledShutdownBindingScopeLocked;
	protected bool m_bControlledShutdownQuiescenceApplied;
	protected bool m_bControlledShutdownQuiescenceExact;
	protected int m_iControlledShutdownQuiescedRoots;
	protected ref HST_CampaignState m_ControlledShutdownState;
	protected ref array<ref HST_ControlledShutdownFieldVehiclePin> m_aControlledShutdownPins = {};

	bool Track(
		IEntity entity,
		HST_RuntimeVehicleState record,
		bool controlledShutdownSnapshotAuthority = false)
	{
		if (!entity || !IsDurableRecord(record))
			return false;
		// Once shutdown publishes its immutable field fence, callers may only
		// re-observe an already pinned exact binding. No shared registration seam
		// may expand or replace the frozen scope.
		if (m_bControlledShutdownQuiescenceApplied)
			return HasExactBinding(entity, record);
		if (m_bControlledShutdownBindingScopeLocked
			&& !controlledShutdownSnapshotAuthority)
			return HasExactBinding(entity, record);
		if (controlledShutdownSnapshotAuthority
			&& !m_bControlledShutdownBindingScopeLocked)
			return false;
		if (m_aEntities.Count() != m_aRuntimeIds.Count())
		{
			m_bBindingFault = true;
			return false;
		}
		int entityIndex = m_aEntities.Find(entity);
		int idIndex = m_aRuntimeIds.Find(record.m_sVehicleRuntimeId);
		if (entityIndex >= 0)
		{
			if (m_aRuntimeIds[entityIndex] != record.m_sVehicleRuntimeId
				|| idIndex != entityIndex)
			{
				m_bBindingFault = true;
				Print("Partisan vehicle persistence | rejected durable root ID rebind/collision", LogLevel.ERROR);
				return false;
			}
		}
		else if (idIndex >= 0)
		{
			IEntity existingEntity = m_aEntities[idIndex];
			if (existingEntity && existingEntity != entity
				&& IsLivingEntity(existingEntity))
			{
				m_bBindingFault = true;
				Print("Partisan vehicle persistence | rejected durable ID/live-root collision", LogLevel.ERROR);
				return false;
			}
		}

		// Durable Partisan vehicle rows are the sole restart authority. Stock
		// vehicle prefabs also carry native entity persistence; leaving that live
		// would create a second record for the same root and can make a blocking
		// shutdown flush wait on both authorities. Remove any native row before
		// publishing the exact HST binding. removeData=true also retires an older
		// native row instead of allowing it to respawn beside this durable ledger.
		if (!DetachNativePersistenceAuthority(entity))
			return false;
		if (entityIndex >= 0)
			return true;
		if (idIndex >= 0)
		{
			m_aEntities[idIndex] = entity;
			return true;
		}
		m_aEntities.Insert(entity);
		m_aRuntimeIds.Insert(record.m_sVehicleRuntimeId);
		return true;
	}

	bool LockControlledShutdownBindingScope()
	{
		m_bControlledShutdownBindingScopeLocked = true;
		return true;
	}

	bool HasControlledShutdownBindingScopeLocked()
	{
		return m_bControlledShutdownBindingScopeLocked;
	}

	bool HasExactBinding(
		IEntity entity,
		HST_RuntimeVehicleState record,
		HST_CampaignState state = null)
	{
		if (!entity || !entity.GetWorld() || !IsLivingEntity(entity)
			|| !IsDurableRecord(record) || m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count())
			return false;
		if (state && state.FindRuntimeVehicle(record.m_sVehicleRuntimeId) != record)
			return false;
		int entityIndex = m_aEntities.Find(entity);
		int runtimeIndex = m_aRuntimeIds.Find(record.m_sVehicleRuntimeId);
		if (entityIndex < 0 || entityIndex != runtimeIndex
			|| entityIndex >= m_aEntities.Count()
			|| runtimeIndex >= m_aRuntimeIds.Count()
			|| m_aEntities[entityIndex] != entity
			|| m_aRuntimeIds[runtimeIndex] != record.m_sVehicleRuntimeId)
			return false;
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence && (persistence.IsTracked(entity)
			|| persistence.GetTrackedParent(entity) != null))
			return false;
		if (m_bControlledShutdownQuiescenceApplied)
		{
			foreach (HST_ControlledShutdownFieldVehiclePin pin : m_aControlledShutdownPins)
			{
				if (pin && pin.m_sRuntimeId == record.m_sVehicleRuntimeId)
					return pin.m_Entity == entity && pin.m_Record == record;
			}
			return false;
		}
		return entityIndex >= 0 && entityIndex == runtimeIndex
			&& entityIndex < m_aEntities.Count()
			&& runtimeIndex < m_aRuntimeIds.Count()
			&& m_aEntities[entityIndex] == entity
			&& m_aRuntimeIds[runtimeIndex] == record.m_sVehicleRuntimeId;
	}

	int CountNativeTrackedDurableRoots(HST_CampaignState state)
	{
		if (!state || m_aEntities.Count() != m_aRuntimeIds.Count())
			return -1;
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return 0;
		int trackedRoots;
		for (int index = 0; index < m_aRuntimeIds.Count(); index++)
		{
			HST_RuntimeVehicleState record = state.FindRuntimeVehicle(
				m_aRuntimeIds[index]);
			if (!IsDurableRecord(record))
				continue;
			IEntity entity = m_aEntities[index];
			if (entity && (persistence.IsTracked(entity)
				|| persistence.GetTrackedParent(entity) != null))
				trackedRoots++;
		}
		return trackedRoots;
	}

	bool RetireNativeTrackedInactiveRoot(
		IEntity entity,
		HST_RuntimeVehicleState record)
	{
		if (!entity || !entity.GetWorld() || !record
			|| (!record.m_bDeleted && !record.m_bDetached)
			|| !BindingMatchesRecord(entity, record)
			|| m_aEntities.Find(entity) >= 0
			|| HasLivingPlayerOccupant(entity))
			return false;
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return false;
		IEntity trackedParent = persistence.GetTrackedParent(entity);
		if ((trackedParent && trackedParent != entity)
			|| !persistence.IsTracked(entity)
			|| !persistence.StopTracking(entity, true)
			|| persistence.IsTracked(entity)
			|| persistence.GetTrackedParent(entity) != null)
			return false;
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		return !entity.GetWorld();
	}

	// This pass is deliberately read-only. The first invocation can tolerate a
	// moved durable root because PrepareForCapture still owns folding its live
	// transform into durable authority. The strict invocation immediately after
	// that fold requires the record/root transform and native tracking topology to
	// be exact before any active-group or field-vehicle mutation is admitted.
	bool PreflightControlledShutdownQuiescence(
		HST_CampaignState state,
		out string evidence,
		bool requirePreparedAuthority = false)
	{
		if (m_bControlledShutdownQuiescenceApplied)
			return ValidateControlledShutdownFenceIdentity(state, evidence);
		return ValidateControlledShutdownCandidateTopology(
			state,
			requirePreparedAuthority,
				evidence);
	}

	// Read-only gate shared by controlled-shutdown domain discovery. These roots
	// are not tracked or detached here: the caller can establish its complete
	// candidate set before any shutdown-domain mutation starts.
	bool ValidateControlledShutdownSnapshotCandidateRoots(
		array<IEntity> roots,
		out string evidence)
	{
		evidence
			= "durable field vehicle controlled-shutdown snapshot candidate topology rejected";
		if (!roots || m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count()
			|| HasBindingCollision())
			return false;

		array<IEntity> uniqueRoots = {};
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		foreach (IEntity root : roots)
		{
			if (!root || !root.GetWorld() || !IsLivingEntity(root)
				|| uniqueRoots.Find(root) >= 0
				|| IsControlledShutdownRuntimeProxy(root)
				|| !ValidateControlledShutdownHierarchyLocality(root, evidence))
			{
				if (evidence.IsEmpty())
				{
					evidence
						= "durable field vehicle controlled-shutdown snapshot candidate root/locality topology changed";
				}
				return false;
			}
			if (persistence)
			{
				IEntity trackedParent = persistence.GetTrackedParent(root);
				if (trackedParent && trackedParent != root)
				{
					evidence
						= "durable field vehicle controlled-shutdown snapshot candidate belongs to another native persistence root";
					return false;
				}
			}
			uniqueRoots.Insert(root);
		}

		if (!ValidateControlledShutdownPlayerSafety(uniqueRoots, evidence))
			return false;
		evidence = string.Format(
			"durable field vehicle controlled-shutdown snapshot candidate topology exact | roots %1",
			uniqueRoots.Count());
		return true;
	}

	bool PrepareForControlledShutdown(
		HST_CampaignState state,
		out string evidence)
	{
		evidence = "durable field vehicle controlled-shutdown quiescence rejected";
		if (m_bControlledShutdownQuiescenceApplied)
			return MaintainControlledShutdownQuiescence(state, evidence);

		// Persistence already ran PrepareStateForCapture and a strict read-only
		// preflight in the same script turn. Recheck it here before pinning, then set
		// the irreversible latch before the first root transform/controller mutation.
		if (!ValidateControlledShutdownCandidateTopology(state, true, evidence)
			|| !BuildControlledShutdownFencePins(state, evidence))
			return false;
		m_bControlledShutdownQuiescenceApplied = true;
		m_bControlledShutdownQuiescenceExact = false;
		m_iControlledShutdownQuiescedRoots = 0;
		return MaintainControlledShutdownQuiescence(state, evidence);
	}

	bool MaintainControlledShutdownQuiescence(
		HST_CampaignState state,
		out string evidence)
	{
		evidence
			= "durable field vehicle controlled-shutdown quiescence maintenance rejected";
		if (!m_bControlledShutdownQuiescenceApplied
			|| !ValidateControlledShutdownFenceIdentity(state, evidence))
			return false;

		m_bControlledShutdownQuiescenceExact = false;
		m_iControlledShutdownQuiescedRoots = 0;
		foreach (HST_ControlledShutdownFieldVehiclePin pin : m_aControlledShutdownPins)
		{
			if (!ApplyControlledShutdownFencePin(pin, evidence))
				return false;
			m_iControlledShutdownQuiescedRoots++;
		}
		m_bControlledShutdownQuiescenceExact
			= m_iControlledShutdownQuiescedRoots
				== m_aControlledShutdownPins.Count();
		if (!m_bControlledShutdownQuiescenceExact
			|| !IsControlledShutdownQuiescenceExact(state))
			return false;
		evidence = string.Format(
			"durable field vehicle controlled-shutdown quiescence maintained for %1 pinned root(s)",
			m_iControlledShutdownQuiescedRoots);
		return true;
	}

	bool HasControlledShutdownQuiescenceApplied()
	{
		return m_bControlledShutdownQuiescenceApplied;
	}

	bool IsControlledShutdownQuiescenceExact(HST_CampaignState state)
	{
		if (!m_bControlledShutdownQuiescenceApplied
			|| !m_bControlledShutdownQuiescenceExact
			|| !state
			|| m_iControlledShutdownQuiescedRoots
				!= m_aControlledShutdownPins.Count())
			return false;
		string evidence;
		if (!ValidateControlledShutdownFenceIdentity(state, evidence))
			return false;
		foreach (HST_ControlledShutdownFieldVehiclePin pin : m_aControlledShutdownPins)
		{
			if (!pin || !IsControlledShutdownRootAtPinnedTransform(pin)
				|| !IsControlledShutdownRootQuiescent(pin.m_Entity))
				return false;
		}
		return true;
	}

	int GetControlledShutdownQuiescedRootCount()
	{
		return m_iControlledShutdownQuiescedRoots;
	}

	bool Untrack(IEntity entity = null, string runtimeId = "")
	{
		if (m_bControlledShutdownBindingScopeLocked
			|| m_bControlledShutdownQuiescenceApplied)
			return false;
		bool changed;
		for (int index = m_aRuntimeIds.Count() - 1; index >= 0; index--)
		{
			if ((!entity || index >= m_aEntities.Count()
					|| m_aEntities[index] != entity)
				&& (runtimeId.IsEmpty()
					|| m_aRuntimeIds[index] != runtimeId))
				continue;
			RemoveAt(index);
			changed = true;
		}
		m_bBindingFault = m_bBindingFault || HasBindingCollision();
		return changed;
	}

	HST_RuntimeVehicleState ResolveForEntity(
		HST_CampaignState state,
		IEntity entity)
	{
		if (!state || !entity)
			return null;
		int trackedIndex = m_aEntities.Find(entity);
		if (trackedIndex >= 0 && trackedIndex < m_aRuntimeIds.Count())
		{
			HST_RuntimeVehicleState tracked = state.FindRuntimeVehicle(
				m_aRuntimeIds[trackedIndex]);
			if (IsDurableRecord(tracked))
				return tracked;
			// A registered root whose durable row vanished is an authority fault,
			// not permission to borrow a nearby row under a different ID.
			return null;
		}

		string prefab;
		if (entity.GetPrefabData())
			prefab = entity.GetPrefabData().GetPrefabName();
		vector origin = entity.GetOrigin();
		HST_RuntimeVehicleState positionalMatch;
		foreach (HST_RuntimeVehicleState candidate : state.m_aRuntimeVehicles)
		{
			if (!IsDurableRecord(candidate) || candidate.m_sPrefab != prefab)
				continue;
			if (DistanceSq2D(candidate.m_vPosition, origin) > 64.0)
				continue;
			int candidateBindingIndex
				= m_aRuntimeIds.Find(candidate.m_sVehicleRuntimeId);
			if (candidateBindingIndex >= 0
				&& candidateBindingIndex < m_aEntities.Count()
				&& m_aEntities[candidateBindingIndex] != entity)
				continue;
			// Positional lookup is migration/recovery fallback only. Two plausible
			// rows are ambiguous, so fail closed instead of binding the first one.
			if (positionalMatch && positionalMatch != candidate)
				return null;
			positionalMatch = candidate;
		}
		return positionalMatch;
	}

	IEntity ResolveEntityForRuntimeId(string runtimeId)
	{
		if (runtimeId.IsEmpty()
			|| m_aEntities.Count() != m_aRuntimeIds.Count())
			return null;
		int index = m_aRuntimeIds.Find(runtimeId);
		if (index < 0 || index >= m_aEntities.Count())
			return null;
		IEntity entity = m_aEntities[index];
		if (!IsLivingEntity(entity))
			return null;
		return entity;
	}

	bool PrepareForCapture(HST_CampaignState state)
	{
		// A controlled-end fence is one-way. Once any root mutation begins, all
		// later capture preparation must reapply the original pins; it must never
		// fold a partially stopped root back into durable authority or prune the
		// immutable binding set.
		if (m_bControlledShutdownQuiescenceApplied)
		{
			string controlledShutdownEvidence;
			return MaintainControlledShutdownQuiescence(
				state,
				controlledShutdownEvidence);
		}
		if (!state || m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count())
			return false;
		for (int index = m_aEntities.Count() - 1; index >= 0; index--)
		{
			IEntity entity = m_aEntities[index];
			string runtimeId = m_aRuntimeIds[index];
			HST_RuntimeVehicleState record = state.FindRuntimeVehicle(runtimeId);
			if (!record)
			{
				if (IsLivingEntity(entity))
					return false;
				RemoveAt(index);
				continue;
			}
			if (!IsDurableRecord(record))
			{
				if (IsLivingEntity(entity))
					return false;
				RemoveAt(index);
				continue;
			}
			if (!entity || !entity.GetWorld())
			{
				UpdateCargoPosition(state, runtimeId, record.m_vPosition);
				record.m_bDeleted = true;
				RemoveAt(index);
				continue;
			}
			// A prefab persistence component can register lazily after Track. Repeat
			// the sole-authority gate at every capture boundary so a late native
			// registration cannot survive into any save type.
			if (!DetachNativePersistenceAuthority(entity))
				return false;

			record.m_vPosition = entity.GetOrigin();
			record.m_vAngles = HST_WorldPositionService.BuildUprightAnglesFromVector(
				entity.GetYawPitchRoll());
			UpdateCargoPosition(state, runtimeId, record.m_vPosition);
			if (!IsLivingEntity(entity))
			{
				if (HasLivingPlayerOccupant(entity))
				{
					Print(
						"Partisan vehicle persistence | destroyed durable root still has a living player occupant",
						LogLevel.ERROR);
					return false;
				}
				record.m_bDeleted = true;
				// A destroyed durable root is represented by the campaign tombstone,
				// not by a lingering physical wreck. Delete it before native capture so
				// a later process cannot observe both authorities.
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				RemoveAt(index);
				continue;
			}
			record.m_bDeleted = false;
		}

		int activeRows;
		int inactiveRows;
		string graphEvidence;
		if (!ValidateDurableStateGraph(
			state,
			true,
			activeRows,
			inactiveRows,
			graphEvidence))
		{
			Print(
				"Partisan vehicle persistence | capture graph rejected: "
					+ graphEvidence,
				LogLevel.ERROR);
			return false;
		}
		return true;
	}

	// Durable field rows form a small identity graph: every durable ID is
	// unique, every active row has one live registered root at capture time, and
	// abstract cargo keys cannot silently collide. Restore uses the same graph
	// gate without requiring process-local bindings before reconstruction.
	bool ValidateDurableStateGraph(
		HST_CampaignState state,
		bool requireActiveBindings,
		out int activeRows,
		out int inactiveRows,
		out string evidence)
	{
		activeRows = 0;
		inactiveRows = 0;
		evidence = "durable field vehicle graph unavailable";
		if (!state || m_aEntities.Count() != m_aRuntimeIds.Count())
			return false;

		array<string> durableIds = {};
		foreach (HST_RuntimeVehicleState record : state.m_aRuntimeVehicles)
		{
			if (!record || !IsDurableRuntimeKind(record.m_sRuntimeKind))
				continue;
			if (record.m_sVehicleRuntimeId.IsEmpty()
				|| durableIds.Find(record.m_sVehicleRuntimeId) >= 0
				|| record.m_sPrefab.IsEmpty())
			{
				evidence = "durable field vehicle ID or prefab is empty/duplicated";
				return false;
			}
			durableIds.Insert(record.m_sVehicleRuntimeId);
			if (record.m_bDeleted || record.m_bDetached)
			{
				inactiveRows++;
				if (requireActiveBindings
					&& m_aRuntimeIds.Find(record.m_sVehicleRuntimeId) >= 0)
				{
					evidence
						= "inactive durable field vehicle retains a physical binding";
					return false;
				}
				continue;
			}

			activeRows++;
			if (IsZeroVector(record.m_vPosition))
			{
				evidence = "active durable field vehicle has a zero transform";
				return false;
			}
			if (!requireActiveBindings)
				continue;
			int bindingIndex = m_aRuntimeIds.Find(record.m_sVehicleRuntimeId);
			if (bindingIndex < 0 || bindingIndex >= m_aEntities.Count()
				|| !IsLivingEntity(m_aEntities[bindingIndex]))
			{
				evidence = "active durable field vehicle has no exact live binding";
				return false;
			}
			if (!BindingMatchesRecord(m_aEntities[bindingIndex], record))
			{
				evidence
					= "active durable field vehicle binding has the wrong prefab or transform";
				return false;
			}
		}

		array<string> cargoKeys = {};
		foreach (HST_VehicleCargoItemState cargo : state.m_aVehicleCargoItems)
		{
			if (!cargo
				|| durableIds.Find(cargo.m_sVehicleRuntimeId) < 0)
				continue;
			if (cargo.m_sItemPrefab.IsEmpty() || cargo.m_iCount <= 0)
			{
				evidence = "durable field vehicle cargo is empty or non-positive";
				return false;
			}
			string cargoKey = cargo.m_sVehicleRuntimeId
				+ "|" + cargo.m_sItemPrefab;
			if (cargoKeys.Find(cargoKey) >= 0)
			{
				evidence = "durable field vehicle cargo key is duplicated";
				return false;
			}
			cargoKeys.Insert(cargoKey);
		}

		if (requireActiveBindings && HasBindingCollision())
		{
			evidence = "durable field vehicle binding graph contains a collision";
			return false;
		}
		evidence = string.Format(
			"durable field vehicle graph exact | active/inactive/cargo %1/%2/%3",
			activeRows,
			inactiveRows,
			cargoKeys.Count());
		return true;
	}

	bool IsDurableCampaignRecord(
		HST_RuntimeVehicleState record,
		bool includeInactive = false)
	{
		if (!record || record.m_sVehicleRuntimeId.IsEmpty()
			|| !IsDurableRuntimeKind(record.m_sRuntimeKind))
			return false;
		return includeInactive || (!record.m_bDeleted && !record.m_bDetached);
	}

	// This is the non-mutating half of reset cleanup. The caller must first run
	// PrepareForCapture (normally through civilian persistence reconciliation),
	// which folds current transforms and removes already-dead bindings. This pass
	// then freezes the exact deletion set only after every retained/removed root
	// has passed its player-safety and identity checks.
	HST_PersistentFieldVehicleNewCampaignResetPlan BuildNewCampaignResetCleanupPlanAfterCapture(
		HST_CampaignState state,
		array<string> retainedRuntimeIds,
		out string evidence)
	{
		evidence = "durable field vehicle new-campaign cleanup plan rejected";
		if (!state || m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count())
			return null;
		int activeRows;
		int inactiveRows;
		string graphEvidence;
		if (!ValidateDurableStateGraph(
			state,
			true,
			activeRows,
			inactiveRows,
			graphEvidence))
		{
			evidence += " | " + graphEvidence;
			return null;
		}

		HST_PersistentFieldVehicleNewCampaignResetPlan plan
			= new HST_PersistentFieldVehicleNewCampaignResetPlan();
		plan.m_State = state;
		if (retainedRuntimeIds)
		{
			foreach (string retainedId : retainedRuntimeIds)
			{
				int retainedIndex = m_aRuntimeIds.Find(retainedId);
				if (retainedId.IsEmpty() || retainedIndex < 0
					|| plan.m_aRetainedRuntimeIds.Find(retainedId) >= 0)
				{
					evidence
						= "durable field vehicle reset retained ID is empty, missing, or duplicated";
					return null;
				}
				HST_RuntimeVehicleState retainedRecord
					= state.FindRuntimeVehicle(retainedId);
				IEntity retainedEntity = m_aEntities[retainedIndex];
				if (!IsDurableRecord(retainedRecord)
					|| !retainedEntity
					|| !HasLivingPlayerOccupant(retainedEntity))
				{
					evidence
						= "durable field vehicle reset retained root lost exact living player occupancy";
					return null;
				}
				plan.m_aRetainedRuntimeIds.Insert(retainedId);
			}
		}

		for (int index; index < m_aRuntimeIds.Count(); index++)
		{
			string runtimeId = m_aRuntimeIds[index];
			bool retained = plan.m_aRetainedRuntimeIds.Find(runtimeId) >= 0;
			IEntity entity = m_aEntities[index];
			HST_RuntimeVehicleState record = state.FindRuntimeVehicle(runtimeId);
			if (!IsDurableRecord(record) || !entity)
			{
				evidence
					= "durable field vehicle reset binding lost active durable authority after capture";
				return null;
			}
			if (retained && !HasLivingPlayerOccupant(entity))
			{
				evidence
					= "durable field vehicle reset retained root is no longer player occupied";
				return null;
			}
			if (!retained && entity && HasLivingPlayerOccupant(entity))
			{
				evidence
					= "durable field vehicle reset would delete a living player-occupied root";
				return null;
			}
			if (retained)
				continue;
			plan.m_aDeleteRuntimeIds.Insert(runtimeId);
			plan.m_aDeleteEntities.Insert(entity);
			plan.m_aDeleteRecords.Insert(record);
		}
		if (plan.m_aRetainedRuntimeIds.Count()
			+ plan.m_aDeleteRuntimeIds.Count() != m_aRuntimeIds.Count())
		{
			evidence
				= "durable field vehicle reset plan does not cover every exact binding";
			return null;
		}
		plan.m_bPrepared = true;
		evidence = string.Format(
			"durable field vehicle reset plan exact | retained/delete %1/%2",
			plan.m_aRetainedRuntimeIds.Count(),
			plan.m_aDeleteRuntimeIds.Count());
		return plan;
	}

	// Every fallible identity, graph, and occupancy decision belongs to the plan
	// builder. A consumed plan owns a fixed deletion set, so commit has no reject
	// path and cannot strand a half-pruned binding graph.
	void CommitNewCampaignResetCleanupPlan(
		HST_PersistentFieldVehicleNewCampaignResetPlan plan)
	{
		if (!plan || !plan.m_bPrepared || plan.m_bCommitted)
			return;
		for (int deleteIndex; deleteIndex < plan.m_aDeleteRuntimeIds.Count(); deleteIndex++)
		{
			string runtimeId = plan.m_aDeleteRuntimeIds[deleteIndex];
			HST_RuntimeVehicleState record = plan.m_aDeleteRecords[deleteIndex];
			if (record)
				record.m_bDeleted = true;
			IEntity entity = plan.m_aDeleteEntities[deleteIndex];
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			int bindingIndex = m_aRuntimeIds.Find(runtimeId);
			if (bindingIndex >= 0)
				RemoveAt(bindingIndex);
		}
		plan.m_bCommitted = true;
	}

	bool CleanupForNewCampaignReset(
		HST_CampaignState state,
		array<string> retainedRuntimeIds)
	{
		if (!state || !PrepareForCapture(state))
			return false;
		string evidence;
		HST_PersistentFieldVehicleNewCampaignResetPlan plan
			= BuildNewCampaignResetCleanupPlanAfterCapture(
				state,
				retainedRuntimeIds,
				evidence);
		if (!plan)
			return false;
		CommitNewCampaignResetCleanupPlan(plan);
		return true;
	}

	protected bool IsDurableRecord(HST_RuntimeVehicleState record)
	{
		return IsDurableCampaignRecord(record);
	}

	protected bool DetachNativePersistenceAuthority(IEntity entity)
	{
		if (!entity)
			return false;
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return true;
		IEntity trackedParent = persistence.GetTrackedParent(entity);
		if (trackedParent && trackedParent != entity)
		{
			Print(
				"Partisan vehicle persistence | durable root belongs to a native tracked parent",
				LogLevel.ERROR);
			return false;
		}
		if (!persistence.IsTracked(entity))
			return true;
		if (!persistence.StopTracking(entity, true)
			|| persistence.IsTracked(entity))
		{
			Print(
				"Partisan vehicle persistence | native entity authority detachment failed",
				LogLevel.ERROR);
			return false;
		}
		return true;
	}

	protected bool ValidateControlledShutdownCandidateTopology(
		HST_CampaignState state,
		bool requirePreparedAuthority,
		out string evidence)
	{
		evidence
			= "durable field vehicle controlled-shutdown topology preflight rejected";
		if (!state || m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count()
			|| HasBindingCollision())
			return false;

		int activeRows;
		int inactiveRows;
		string graphEvidence;
		if (!ValidateDurableStateGraph(
			state,
			false,
			activeRows,
			inactiveRows,
			graphEvidence)
			|| m_aRuntimeIds.Count() != activeRows)
		{
			evidence += " | " + graphEvidence;
			return false;
		}

		array<IEntity> roots = {};
		for (int index; index < m_aRuntimeIds.Count(); index++)
		{
			string runtimeId = m_aRuntimeIds[index];
			IEntity root = m_aEntities[index];
			HST_RuntimeVehicleState record = state.FindRuntimeVehicle(runtimeId);
			if (!IsDurableRecord(record)
				|| record.m_sVehicleRuntimeId != runtimeId
				|| !root || !root.GetWorld() || !IsLivingEntity(root)
				|| roots.Find(root) >= 0
				|| !BindingPrefabMatchesRecord(root, record)
				|| IsControlledShutdownRuntimeProxy(root)
				|| !ValidateControlledShutdownHierarchyLocality(root, evidence))
			{
				if (evidence.IsEmpty())
				{
					evidence
						= "durable field vehicle controlled-shutdown binding/locality topology changed";
				}
				return false;
			}
			if (requirePreparedAuthority
				&& !PreparedBindingMatchesRecordExact(root, record))
			{
				evidence
					= "durable field vehicle controlled-shutdown prepared transform is not exact ";
				evidence += runtimeId;
				return false;
			}
			roots.Insert(root);
		}
		if (requirePreparedAuthority
			&& CountNativeTrackedDurableRoots(state) != 0)
		{
			evidence
				= "durable field vehicle controlled-shutdown native persistence authority remains attached";
			return false;
		}
		if (!ValidateControlledShutdownPlayerSafety(roots, evidence))
			return false;
		evidence = string.Format(
			"durable field vehicle controlled-shutdown topology preflight exact | prepared/active/inactive %1/%2/%3",
			requirePreparedAuthority,
			activeRows,
			inactiveRows);
		return true;
	}

	protected bool BuildControlledShutdownFencePins(
		HST_CampaignState state,
		out string evidence)
	{
		evidence
			= "durable field vehicle controlled-shutdown fence pinning rejected";
		if (m_bControlledShutdownQuiescenceApplied || !state)
			return false;
		m_ControlledShutdownState = null;
		m_aControlledShutdownPins.Clear();
		foreach (HST_RuntimeVehicleState record : state.m_aRuntimeVehicles)
		{
			if (!IsDurableRecord(record))
				continue;
			int bindingIndex = m_aRuntimeIds.Find(record.m_sVehicleRuntimeId);
			if (bindingIndex < 0 || bindingIndex >= m_aEntities.Count())
			{
				m_aControlledShutdownPins.Clear();
				return false;
			}
			IEntity root = m_aEntities[bindingIndex];
			if (!root || !PreparedBindingMatchesRecordExact(root, record))
			{
				m_aControlledShutdownPins.Clear();
				return false;
			}
			HST_ControlledShutdownFieldVehiclePin pin
				= new HST_ControlledShutdownFieldVehiclePin();
			pin.m_sRuntimeId = record.m_sVehicleRuntimeId;
			pin.m_Entity = root;
			pin.m_Record = record;
			pin.m_vDurablePosition = record.m_vPosition;
			pin.m_vDurableAngles
				= HST_WorldPositionService.BuildUprightAnglesFromVector(
					record.m_vAngles);
			m_aControlledShutdownPins.Insert(pin);
		}
		if (m_aControlledShutdownPins.Count() != m_aRuntimeIds.Count())
		{
			m_aControlledShutdownPins.Clear();
			return false;
		}
		m_ControlledShutdownState = state;
		evidence = string.Format(
			"durable field vehicle controlled-shutdown pinned %1 original durable root(s)",
			m_aControlledShutdownPins.Count());
		return true;
	}

	protected bool ValidateControlledShutdownFenceIdentity(
		HST_CampaignState state,
		out string evidence)
	{
		evidence
			= "durable field vehicle controlled-shutdown pinned authority changed";
		if (!m_bControlledShutdownQuiescenceApplied || !state
			|| state != m_ControlledShutdownState
			|| m_bBindingFault
			|| m_aEntities.Count() != m_aRuntimeIds.Count()
			|| m_aControlledShutdownPins.Count() != m_aRuntimeIds.Count()
			|| CountNativeTrackedDurableRoots(state) != 0
			|| HasBindingCollision())
			return false;

		int activeRows;
		int inactiveRows;
		string graphEvidence;
		if (!ValidateDurableStateGraph(
			state,
			false,
			activeRows,
			inactiveRows,
			graphEvidence)
			|| activeRows != m_aControlledShutdownPins.Count())
		{
			evidence += " | " + graphEvidence;
			return false;
		}

		array<IEntity> roots = {};
		foreach (HST_ControlledShutdownFieldVehiclePin pin : m_aControlledShutdownPins)
		{
			if (!ValidateControlledShutdownFencePinIdentity(pin, evidence))
				return false;
			roots.Insert(pin.m_Entity);
		}
		if (!ValidateControlledShutdownPlayerSafety(roots, evidence))
			return false;
		evidence = string.Format(
			"durable field vehicle controlled-shutdown pinned authority exact | active/inactive %1/%2",
			activeRows,
			inactiveRows);
		return true;
	}

	protected bool ValidateControlledShutdownFencePinIdentity(
		HST_ControlledShutdownFieldVehiclePin pin,
		out string evidence)
	{
		if (!pin || pin.m_sRuntimeId.IsEmpty() || !pin.m_Record
			|| !pin.m_Entity || !pin.m_Entity.GetWorld()
			|| !IsLivingEntity(pin.m_Entity)
			|| m_aRuntimeIds.Find(pin.m_sRuntimeId) < 0)
			return false;
		int bindingIndex = m_aRuntimeIds.Find(pin.m_sRuntimeId);
		if (bindingIndex >= m_aEntities.Count()
			|| m_aEntities[bindingIndex] != pin.m_Entity
			|| m_ControlledShutdownState.FindRuntimeVehicle(pin.m_sRuntimeId)
				!= pin.m_Record
			|| !IsDurableRecord(pin.m_Record)
			|| !BindingPrefabMatchesRecord(pin.m_Entity, pin.m_Record)
			|| !IsControlledShutdownRecordAtPinnedTransform(pin)
			|| IsControlledShutdownRuntimeProxy(pin.m_Entity)
			|| !ValidateControlledShutdownHierarchyLocality(pin.m_Entity, evidence))
			return false;
		return true;
	}

	protected bool ApplyControlledShutdownFencePin(
		HST_ControlledShutdownFieldVehiclePin pin,
		out string evidence)
	{
		if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
			return false;
		HST_WorldPositionService.ApplyUprightEntityTransform(
			pin.m_Entity,
			pin.m_vDurablePosition,
			pin.m_vDurableAngles);
		if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence)
			|| !QuiesceControlledShutdownRoot(pin, evidence))
			return false;
		return IsControlledShutdownRootAtPinnedTransform(pin)
			&& IsControlledShutdownRootQuiescent(pin.m_Entity);
	}

	protected bool ValidateControlledShutdownFencePinMutationSafety(
		HST_ControlledShutdownFieldVehiclePin pin,
		out string evidence)
	{
		if (!ValidateControlledShutdownFencePinIdentity(pin, evidence))
			return false;
		array<IEntity> roots = {};
		roots.Insert(pin.m_Entity);
		if (!ValidateControlledShutdownPlayerSafety(roots, evidence))
			return false;
		return ValidateControlledShutdownHierarchyLocality(
			pin.m_Entity,
			evidence);
	}

	protected bool ValidateControlledShutdownPlayerSafety(
		array<IEntity> roots,
		out string evidence)
	{
		if (!roots || roots.Count() == 0)
		{
			evidence
				= "durable field vehicle controlled-shutdown player safety exact";
			return true;
		}
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			evidence
				= HST_PhysicalWarService.CONTROLLED_SHUTDOWN_PLAYER_RELEASE_EVIDENCE
					+ " | durable field vehicle player manager unavailable";
			return false;
		}
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity controlledEntity
				= playerManager.GetPlayerControlledEntity(playerId);
			IEntity mainEntity
				= SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
			if (!IsControlledShutdownPlayerEntitySafeForRoots(
					controlledEntity,
					roots)
				|| (mainEntity != controlledEntity
					&& !IsControlledShutdownPlayerEntitySafeForRoots(
						mainEntity,
						roots)))
			{
				evidence
					= HST_PhysicalWarService.CONTROLLED_SHUTDOWN_PLAYER_RELEASE_EVIDENCE;
				evidence += string.Format(
					" | player %1 overlaps a durable field vehicle or is changing compartments",
					playerId);
				return false;
			}
		}
		evidence
			= "durable field vehicle controlled-shutdown player safety exact";
		return true;
	}

	protected bool IsControlledShutdownPlayerEntitySafeForRoots(
		IEntity playerEntity,
		array<IEntity> roots)
	{
		if (!playerEntity)
			return true;
		if (!playerEntity.GetWorld()
			|| IsControlledShutdownEntityWithinRoots(playerEntity, roots))
			return false;
		IEntity hierarchyVehicle
			= CompartmentAccessComponent.GetVehicleIn(playerEntity);
		if (hierarchyVehicle
			&& IsControlledShutdownEntityWithinRoots(hierarchyVehicle, roots))
			return false;
		SCR_CompartmentAccessComponent access
			= SCR_CompartmentAccessComponent.Cast(
				playerEntity.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return true;
		IEntity accessVehicle = access.GetVehicle();
		if (accessVehicle
			&& IsControlledShutdownEntityWithinRoots(accessVehicle, roots))
			return false;
		// A transition can temporarily expose neither a hierarchy vehicle nor a
		// stable access vehicle. Fail closed for every transition rather than let
		// a root move while ownership is unresolved.
		if (access.IsGettingIn() || access.IsGettingOut()
			|| access.IsSwitchingSeatsAnim())
			return false;
		return true;
	}

	protected bool IsControlledShutdownEntityWithinRoots(
		IEntity entity,
		array<IEntity> roots)
	{
		if (!entity || !roots)
			return false;
		IEntity cursor = entity;
		for (int depth; depth < 64 && cursor; depth++)
		{
			if (roots.Find(cursor) >= 0)
				return true;
			IEntity parent = cursor.GetParent();
			if (parent == cursor)
				return false;
			cursor = parent;
		}
		return false;
	}

	protected bool ValidateControlledShutdownHierarchyLocality(
		IEntity entity,
		out string evidence)
	{
		if (!entity || !entity.GetWorld()
			|| IsControlledShutdownRuntimeProxy(entity)
			|| IsControlledShutdownPlayerEntity(entity))
		{
			evidence
				= "durable field vehicle controlled-shutdown root hierarchy is missing, a player, or a replication proxy";
			return false;
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (!ValidateControlledShutdownHierarchyLocality(child, evidence))
				return false;
			child = child.GetSibling();
		}
		return true;
	}

	protected bool IsControlledShutdownRuntimeProxy(IEntity entity)
	{
		if (!entity)
			return true;
		BaseRplComponent replication = BaseRplComponent.Cast(
			entity.FindComponent(BaseRplComponent));
		return replication && replication.IsProxy();
	}

	protected bool IsControlledShutdownPlayerEntity(IEntity entity)
	{
		if (!entity)
			return false;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return true;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (playerManager.GetPlayerControlledEntity(playerId) == entity
				|| SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId)
					== entity)
				return true;
		}
		return false;
	}

	protected bool ValidateControlledShutdownEntityMutationSafety(
		IEntity entity,
		out string evidence)
	{
		if (!entity || !entity.GetWorld()
			|| IsControlledShutdownRuntimeProxy(entity)
			|| IsControlledShutdownPlayerEntity(entity))
		{
			evidence
				= "durable field vehicle controlled-shutdown refused to mutate a player, proxy, or missing hierarchy entity";
			return false;
		}
		return true;
	}

	protected bool QuiescePhysicsHierarchy(
		IEntity entity,
		out string evidence)
	{
		if (!ValidateControlledShutdownEntityMutationSafety(entity, evidence))
			return false;
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (!QuiescePhysicsHierarchy(child, evidence))
				return false;
			child = child.GetSibling();
		}
		Physics physics = entity.GetPhysics();
		if (!physics || !physics.IsDynamic())
			return true;
		if (!ValidateControlledShutdownEntityMutationSafety(entity, evidence))
			return false;
		physics.ClearForces();
		if (!ValidateControlledShutdownEntityMutationSafety(entity, evidence))
			return false;
		physics.SetVelocity(vector.Zero);
		if (!ValidateControlledShutdownEntityMutationSafety(entity, evidence))
			return false;
		physics.SetAngularVelocity(vector.Zero);
		if (!ValidateControlledShutdownEntityMutationSafety(entity, evidence))
			return false;
		physics.SetActive(ActiveState.INACTIVE);
		return true;
	}

	protected bool QuiesceControlledShutdownRoot(
		HST_ControlledShutdownFieldVehiclePin pin,
		out string evidence)
	{
		if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
			return false;
		IEntity root = pin.m_Entity;
		BaseVehicleControllerComponent controller
			= BaseVehicleControllerComponent.Cast(
				root.FindComponent(BaseVehicleControllerComponent));
		if (controller)
		{
			if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
				return false;
			controller.Shutdown();
			if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
				return false;
			controller.StopEngine(false);
		}
		CarControllerComponent carController = CarControllerComponent.Cast(
			root.FindComponent(CarControllerComponent));
		if (carController)
		{
			if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
				return false;
			carController.SetPersistentHandBrake(true);
		}
		HelicopterControllerComponent helicopterController
			= HelicopterControllerComponent.Cast(
				root.FindComponent(HelicopterControllerComponent));
		if (helicopterController)
		{
			if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
				return false;
			helicopterController.SetPersistentWheelBrake(true);
			if (!ValidateControlledShutdownFencePinMutationSafety(pin, evidence))
				return false;
			helicopterController.SetAutohoverEnabled(true);
		}
		return QuiescePhysicsHierarchy(root, evidence);
	}

	protected bool IsControlledShutdownRecordAtPinnedTransform(
		HST_ControlledShutdownFieldVehiclePin pin)
	{
		if (!pin || !pin.m_Record)
			return false;
		return IsControlledShutdownPositionExact(
				pin.m_Record.m_vPosition,
				pin.m_vDurablePosition)
			&& AreControlledShutdownAnglesExact(
				pin.m_Record.m_vAngles,
				pin.m_vDurableAngles);
	}

	protected bool IsControlledShutdownRootAtPinnedTransform(
		HST_ControlledShutdownFieldVehiclePin pin)
	{
		if (!pin || !pin.m_Entity || !pin.m_Entity.GetWorld())
			return false;
		return IsControlledShutdownPositionExact(
				pin.m_Entity.GetOrigin(),
				pin.m_vDurablePosition)
			&& AreControlledShutdownAnglesExact(
				pin.m_Entity.GetYawPitchRoll(),
				pin.m_vDurableAngles);
	}

	protected bool IsControlledShutdownPositionExact(
		vector first,
		vector second)
	{
		float deltaX = first[0] - second[0];
		float deltaY = first[1] - second[1];
		float deltaZ = first[2] - second[2];
		return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ
			<= 0.0001;
	}

	protected bool AreControlledShutdownAnglesExact(vector first, vector second)
	{
		return IsControlledShutdownAngleExact(first[0], second[0])
			&& IsControlledShutdownAngleExact(first[1], second[1])
			&& IsControlledShutdownAngleExact(first[2], second[2]);
	}

	protected bool IsControlledShutdownAngleExact(float first, float second)
	{
		float difference = Math.AbsFloat(first - second);
		while (difference >= 360.0)
			difference -= 360.0;
		if (difference > 180.0)
			difference = 360.0 - difference;
		return difference <= 0.01;
	}

	protected bool IsControlledShutdownRootQuiescent(IEntity entity)
	{
		if (!entity)
			return false;
		BaseVehicleControllerComponent controller
			= BaseVehicleControllerComponent.Cast(
				entity.FindComponent(BaseVehicleControllerComponent));
		if (controller && controller.IsEngineOn())
			return false;
		CarControllerComponent carController = CarControllerComponent.Cast(
			entity.FindComponent(CarControllerComponent));
		if (carController && !carController.GetPersistentHandBrake())
			return false;
		HelicopterControllerComponent helicopterController
			= HelicopterControllerComponent.Cast(
				entity.FindComponent(HelicopterControllerComponent));
		if (helicopterController
			&& (!helicopterController.GetPersistentWheelBrake()
				|| !helicopterController.GetAutohoverEnabled()))
			return false;
		Physics physics = entity.GetPhysics();
		if (physics && physics.IsDynamic())
		{
			if (physics.IsActive()
				|| !IsControlledShutdownZeroVector(physics.GetVelocity())
				|| !IsControlledShutdownZeroVector(
					physics.GetAngularVelocity()))
				return false;
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (!IsControlledShutdownRootQuiescent(child))
				return false;
			child = child.GetSibling();
		}
		return true;
	}

	protected bool IsControlledShutdownZeroVector(vector value)
	{
		return Math.AbsFloat(value[0]) <= 0.001
			&& Math.AbsFloat(value[1]) <= 0.001
			&& Math.AbsFloat(value[2]) <= 0.001;
	}

	protected bool IsDurableRuntimeKind(string runtimeKind)
	{
		return runtimeKind == "loot_vehicle"
			|| runtimeKind == "field_vehicle"
			|| runtimeKind == "garage_redeploy";
	}

	protected bool BindingMatchesRecord(
		IEntity entity,
		HST_RuntimeVehicleState record)
	{
		if (!BindingPrefabMatchesRecord(entity, record))
			return false;
		vector origin = entity.GetOrigin();
		float dx = origin[0] - record.m_vPosition[0];
		float dy = origin[1] - record.m_vPosition[1];
		float dz = origin[2] - record.m_vPosition[2];
		if (dx * dx + dy * dy + dz * dz > 9.0)
			return false;
		vector actualAngles
			= HST_WorldPositionService.BuildUprightAnglesFromVector(
				entity.GetYawPitchRoll());
		vector expectedAngles
			= HST_WorldPositionService.BuildUprightAnglesFromVector(
				record.m_vAngles);
		float yawDifference = Math.AbsFloat(
			actualAngles[0] - expectedAngles[0]);
		if (yawDifference > 180.0)
			yawDifference = 360.0 - yawDifference;
		return yawDifference <= 3.0;
	}

	// PrepareForCapture projects durable field vehicles upright. Strict shutdown
	// admission therefore requires the exact prepared position and yaw, while
	// allowing live pitch/roll to settle until the fence applies the upright pin.
	protected bool PreparedBindingMatchesRecordExact(
		IEntity entity,
		HST_RuntimeVehicleState record)
	{
		if (!BindingPrefabMatchesRecord(entity, record)
			|| !IsControlledShutdownPositionExact(
				entity.GetOrigin(),
				record.m_vPosition))
			return false;
		vector expectedAngles
			= HST_WorldPositionService.BuildUprightAnglesFromVector(
				record.m_vAngles);
		if (!AreControlledShutdownAnglesExact(
				record.m_vAngles,
				expectedAngles))
			return false;
		vector actualAngles = entity.GetYawPitchRoll();
		return IsControlledShutdownAngleExact(
			actualAngles[0],
			expectedAngles[0]);
	}

	protected bool BindingPrefabMatchesRecord(
		IEntity entity,
		HST_RuntimeVehicleState record)
	{
		if (!entity || !record || !entity.GetPrefabData())
			return false;
		string entityPrefab = NormalizePrefabResource(
			entity.GetPrefabData().GetPrefabName());
		string recordPrefab = NormalizePrefabResource(record.m_sPrefab);
		return !entityPrefab.IsEmpty() && entityPrefab == recordPrefab;
	}

	protected string NormalizePrefabResource(string prefab)
	{
		string normalized = prefab.Trim();
		int boundary = normalized.IndexOf("}");
		if (boundary >= 0 && boundary + 1 < normalized.Length())
		{
			normalized = normalized.Substring(
				boundary + 1,
				normalized.Length() - boundary - 1);
		}
		normalized.ToLower();
		return normalized;
	}

	protected void RemoveAt(int index)
	{
		if (index < 0 || index >= m_aEntities.Count()
			|| index >= m_aRuntimeIds.Count())
			return;
		m_aEntities.Remove(index);
		m_aRuntimeIds.Remove(index);
	}

	protected bool HasBindingCollision()
	{
		if (m_aEntities.Count() != m_aRuntimeIds.Count())
			return true;
		for (int first = 0; first < m_aRuntimeIds.Count(); first++)
		{
			for (int second = first + 1; second < m_aRuntimeIds.Count(); second++)
			{
				if (m_aRuntimeIds[first] == m_aRuntimeIds[second]
					|| (m_aEntities[first]
						&& m_aEntities[first] == m_aEntities[second]))
					return true;
			}
		}
		return false;
	}

	protected void UpdateCargoPosition(
		HST_CampaignState state,
		string runtimeId,
		vector position)
	{
		if (!state || runtimeId.IsEmpty())
			return;
		foreach (HST_VehicleCargoItemState cargoItem : state.m_aVehicleCargoItems)
		{
			if (cargoItem && cargoItem.m_sVehicleRuntimeId == runtimeId)
				cargoItem.m_vLastVehiclePosition = position;
		}
	}

	protected bool HasLivingPlayerOccupant(IEntity vehicleEntity)
	{
		if (!vehicleEntity || !vehicleEntity.GetWorld())
			return false;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity controlledEntity
				= playerManager.GetPlayerControlledEntity(playerId);
			IEntity mainEntity
				= SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
			if (IsLivingPlayerEntityUsingVehicle(
					controlledEntity,
					vehicleEntity)
				|| (mainEntity != controlledEntity
					&& IsLivingPlayerEntityUsingVehicle(
						mainEntity,
						vehicleEntity)))
				return true;
		}
		return false;
	}

	protected bool IsLivingPlayerEntityUsingVehicle(
		IEntity playerEntity,
		IEntity vehicleEntity)
	{
		if (!playerEntity || !vehicleEntity || !IsLivingEntity(playerEntity))
			return false;
		if (playerEntity == vehicleEntity)
			return true;
		SCR_CompartmentAccessComponent access
			= SCR_CompartmentAccessComponent.Cast(
				playerEntity.FindComponent(SCR_CompartmentAccessComponent));
		return access && access.IsInCompartment()
			&& access.GetVehicle() == vehicleEntity;
	}

	protected bool IsLivingEntity(IEntity entity)
	{
		if (!entity || !entity.GetWorld())
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
		{
			CharacterControllerComponent controller
				= character.GetCharacterController();
			return controller
				&& controller.GetLifeState() != ECharacterLifeState.DEAD;
		}
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(
			entity.FindComponent(SCR_DamageManagerComponent));
		return !damageManager
			|| damageManager.GetState() != EDamageState.DESTROYED;
	}

	protected float DistanceSq2D(vector first, vector second)
	{
		float dx = first[0] - second[0];
		float dz = first[2] - second[2];
		return dx * dx + dz * dz;
	}

	protected bool IsZeroVector(vector value)
	{
		return value[0] == 0 && value[1] == 0 && value[2] == 0;
	}
}
