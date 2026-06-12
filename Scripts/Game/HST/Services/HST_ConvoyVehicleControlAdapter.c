class HST_ConvoyVehicleControlAdapter
{
	static const string CONVOY_WAYPOINT_PREFAB = "{FBA8DC8FDA0E770D}Prefabs/AI/Waypoints/AIWaypoint_Patrol_Hierarchy.et";

	bool TryBindCrewToVehicle(HST_ActiveGroupState groupState, IEntity groupEntity, IEntity vehicleEntity, out string reason)
	{
		reason = "";
		if (!groupState)
		{
			reason = "Convoy adapter cannot bind crew: group state missing.";
			return false;
		}
		if (!groupEntity)
		{
			reason = "Convoy adapter cannot bind crew: crew group entity missing.";
			return false;
		}
		if (!vehicleEntity)
		{
			reason = "Convoy adapter cannot bind crew: vehicle entity missing.";
			return false;
		}
		if (CountLivingCrew(groupEntity) <= 0)
		{
			reason = "Convoy adapter cannot bind crew yet: crew group has not populated living agents.";
			return false;
		}

		string mobileReason;
		if (!IsVehicleMobile(vehicleEntity, mobileReason))
		{
			reason = "Convoy adapter cannot bind crew: " + mobileReason;
			return false;
		}

		reason = "Convoy crew seating is planned for Phase 6; crew remains near vehicle for static ambush.";
		return false;
	}

	bool TryAssignVehicleRoute(HST_ActiveGroupState groupState, IEntity groupEntity, IEntity vehicleEntity, array<vector> waypoints, out string reason)
	{
		reason = "";
		if (!groupState)
		{
			reason = "Convoy adapter cannot assign route: group state missing.";
			return false;
		}
		if (!groupEntity)
		{
			reason = "Convoy adapter cannot assign route: crew group entity missing.";
			return false;
		}
		if (!vehicleEntity)
		{
			reason = "Convoy adapter cannot assign route: vehicle entity missing.";
			return false;
		}
		if (!waypoints || waypoints.Count() == 0)
		{
			reason = "Convoy adapter cannot assign route: no route waypoints.";
			return false;
		}
		if (CountLivingCrew(groupEntity) <= 0)
		{
			reason = "Convoy adapter cannot assign route: no living crew agents.";
			return false;
		}

		string mobileReason;
		if (!IsVehicleMobile(vehicleEntity, mobileReason))
		{
			reason = "Convoy adapter cannot assign route: " + mobileReason;
			return false;
		}
		if (!HasLivingDriver(groupEntity, vehicleEntity))
		{
			reason = "Convoy adapter cannot assign route: no living driver is bound to the vehicle; real seating is planned for Phase 6.";
			return false;
		}

		AIGroup group = AIGroup.Cast(groupEntity);
		if (!group)
		{
			reason = "Convoy adapter cannot assign route: crew entity is not an AIGroup.";
			return false;
		}

		foreach (vector waypointPosition : waypoints)
		{
			if (!SpawnAndAssignWaypoint(group, waypointPosition))
			{
				reason = "Convoy adapter route assignment failed while creating AI waypoint.";
				return false;
			}
		}

		reason = "Convoy adapter assigned route waypoints.";
		return true;
	}

	int CountLivingCrew(IEntity groupEntity)
	{
		if (!groupEntity)
			return 0;

		AIGroup group = AIGroup.Cast(groupEntity);
		if (group)
			return Math.Max(0, group.GetAgentsCount());

		if (IsLivingEntity(groupEntity))
			return 1;

		return 0;
	}

	bool HasLivingDriver(IEntity groupEntity, IEntity vehicleEntity)
	{
		if (!groupEntity || !vehicleEntity)
			return false;
		if (CountLivingCrew(groupEntity) <= 0)
			return false;

		return false;
	}

	bool IsVehicleMobile(IEntity vehicleEntity, out string reason)
	{
		reason = "";
		if (!vehicleEntity)
		{
			reason = "vehicle entity missing";
			return false;
		}
		if (!IsLivingEntity(vehicleEntity))
		{
			reason = "vehicle is destroyed";
			return false;
		}

		reason = "vehicle entity is alive; detailed mobility checks are planned for later convoy phases";
		return true;
	}

	protected bool SpawnAndAssignWaypoint(AIGroup group, vector position)
	{
		if (!group)
			return false;

		ResourceName waypointResource = CONVOY_WAYPOINT_PREFAB;
		Resource loaded = Resource.Load(waypointResource);
		if (!loaded || !loaded.IsValid())
			return false;

		vector waypointPosition = HST_WorldPositionService.ResolveSafeGroundPosition(position, HST_WorldPositionService.CHARACTER_GROUND_OFFSET, true, 8.0);
		GenericEntity waypointEntity = HST_WorldPositionService.SpawnPrefab(CONVOY_WAYPOINT_PREFAB, waypointPosition, "0 0 0");
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (!waypoint)
			return false;

		group.AddWaypoint(waypoint);
		return true;
	}

	protected bool IsLivingEntity(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		return !damageManager || damageManager.GetState() != EDamageState.DESTROYED;
	}
}
