class HST_VehicleCapabilityPolicy
{
	static string ResolveSourceVehicleKind(string prefab)
	{
		if (IsAmmoSourcePrefab(prefab))
			return "ammo";

		if (IsRepairSourcePrefab(prefab))
			return "repair";

		if (IsFuelSourcePrefab(prefab))
			return "fuel";

		if (IsArmedVehiclePrefab(prefab))
			return "armed";

		return "transport";
	}

	static string ResolveSourceVehicleKindFromState(string prefab, bool ammoSource, bool repairSource, bool fuelSource)
	{
		if (ammoSource)
			return "ammo";

		if (repairSource)
			return "repair";

		if (fuelSource)
			return "fuel";

		return ResolveSourceVehicleKind(prefab);
	}

	static bool IsAmmoSourcePrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		string basename = HST_VehicleRootPolicy.GetResourceBasename(prefab);
		return basename.Contains("Ammo")
			|| basename.Contains("ammo")
			|| basename.Contains("Ammunition")
			|| basename.Contains("ammunition");
	}

	static bool IsRepairSourcePrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		string basename = HST_VehicleRootPolicy.GetResourceBasename(prefab);
		return basename.Contains("Repair")
			|| basename.Contains("repair")
			|| basename.Contains("Service")
			|| basename.Contains("service")
			|| basename.Contains("Maintenance")
			|| basename.Contains("maintenance")
			|| basename.Contains("Workshop")
			|| basename.Contains("workshop");
	}

	static bool IsFuelSourcePrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		string basename = HST_VehicleRootPolicy.GetResourceBasename(prefab);
		return basename.Contains("Fuel")
			|| basename.Contains("fuel")
			|| basename.Contains("Refuel")
			|| basename.Contains("refuel")
			|| basename.Contains("Tanker")
			|| basename.Contains("tanker");
	}

	static bool IsArmedVehiclePrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		string basename = HST_VehicleRootPolicy.GetResourceBasename(prefab);
		return basename.Contains("APC")
			|| basename.Contains("BTR")
			|| basename.Contains("BMP")
			|| basename.Contains("BRDM")
			|| basename.Contains("DShK")
			|| basename.Contains("PKM")
			|| basename.Contains("M2")
			|| basename.Contains("Armed")
			|| basename.Contains("armed");
	}

	static void ApplyToGarageVehicle(HST_GarageVehicleState vehicle)
	{
		if (!vehicle)
			return;

		vehicle.m_bAmmoSource = IsAmmoSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_bRepairSource = IsRepairSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_bFuelSource = IsFuelSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_sSourceVehicleKind = ResolveSourceVehicleKind(vehicle.m_sPrefab);
	}

	static void ApplyToRuntimeVehicle(HST_RuntimeVehicleState vehicle)
	{
		if (!vehicle)
			return;

		vehicle.m_bAmmoSource = IsAmmoSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_bRepairSource = IsRepairSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_bFuelSource = IsFuelSourcePrefab(vehicle.m_sPrefab);
		vehicle.m_sSourceVehicleKind = ResolveSourceVehicleKind(vehicle.m_sPrefab);
	}

	static bool CanGarageVehicleProvideCivilianUndercover(HST_GarageVehicleState vehicle)
	{
		if (!vehicle)
			return false;

		if (vehicle.m_sSourceFactionKey == "CIV")
			return true;
		if (vehicle.m_sSourceVehicleKind.Contains("CIVILIAN") || vehicle.m_sSourceVehicleKind.Contains("CIV"))
			return true;
		if (vehicle.m_sPrefab.Contains("CIV") || vehicle.m_sPrefab.Contains("S105") || vehicle.m_sPrefab.Contains("S1203"))
			return true;

		return false;
	}

	static bool CanRuntimeVehicleProvideCivilianUndercover(HST_RuntimeVehicleState vehicle)
	{
		if (!vehicle || vehicle.m_bDeleted)
			return false;

		if (vehicle.m_sFactionKey == "CIV")
			return true;
		if (vehicle.m_sRuntimeKind.Contains("CIVILIAN") || vehicle.m_sRuntimeKind.Contains("CIV"))
			return true;
		if (vehicle.m_sPrefab.Contains("CIV") || vehicle.m_sPrefab.Contains("S105") || vehicle.m_sPrefab.Contains("S1203"))
			return true;

		return false;
	}

	static void ApplyUndercoverToGarageVehicle(HST_GarageVehicleState vehicle)
	{
		if (!vehicle)
			return;

		if (!vehicle.m_bCanProvideUndercover)
			vehicle.m_bCanProvideUndercover = CanGarageVehicleProvideCivilianUndercover(vehicle);
		NormalizeGarageVehicleCoverState(vehicle);
	}

	static void ApplyUndercoverToRuntimeVehicle(HST_RuntimeVehicleState vehicle)
	{
		if (!vehicle)
			return;

		if (!vehicle.m_bCanProvideUndercover)
			vehicle.m_bCanProvideUndercover = CanRuntimeVehicleProvideCivilianUndercover(vehicle);
		NormalizeRuntimeVehicleCoverState(vehicle);
	}

	static void CopyRuntimeCoverStateToGarage(HST_RuntimeVehicleState source, HST_GarageVehicleState target)
	{
		if (!source || !target)
			return;

		target.m_bReported = source.m_bReported;
		target.m_bCanProvideUndercover = source.m_bCanProvideUndercover || CanGarageVehicleProvideCivilianUndercover(target);
		target.m_iVehicleHeat = source.m_iVehicleHeat;
		target.m_iLastReportedSecond = source.m_iLastReportedSecond;
		target.m_iReportedUntilSecond = source.m_iReportedUntilSecond;
		target.m_iLastVehicleHeatChangedSecond = source.m_iLastVehicleHeatChangedSecond;
		target.m_iPassengerCompromiseCount = source.m_iPassengerCompromiseCount;
		target.m_sLastReportedReason = source.m_sLastReportedReason;
		target.m_sLastReporterZoneId = source.m_sLastReporterZoneId;
		NormalizeGarageVehicleCoverState(target);
	}

	static void CopyGarageCoverStateToRuntime(HST_GarageVehicleState source, HST_RuntimeVehicleState target)
	{
		if (!source || !target)
			return;

		target.m_bReported = source.m_bReported;
		target.m_bCanProvideUndercover = source.m_bCanProvideUndercover || CanRuntimeVehicleProvideCivilianUndercover(target);
		target.m_iVehicleHeat = source.m_iVehicleHeat;
		target.m_iLastReportedSecond = source.m_iLastReportedSecond;
		target.m_iReportedUntilSecond = source.m_iReportedUntilSecond;
		target.m_iLastVehicleHeatChangedSecond = source.m_iLastVehicleHeatChangedSecond;
		target.m_iPassengerCompromiseCount = source.m_iPassengerCompromiseCount;
		target.m_sLastReportedReason = source.m_sLastReportedReason;
		target.m_sLastReporterZoneId = source.m_sLastReporterZoneId;
		NormalizeRuntimeVehicleCoverState(target);
	}

	static void NormalizeGarageVehicleCoverState(HST_GarageVehicleState vehicle)
	{
		if (!vehicle)
			return;

		vehicle.m_iVehicleHeat = Math.Max(0, Math.Min(10, vehicle.m_iVehicleHeat));
		vehicle.m_iPassengerCompromiseCount = Math.Max(0, vehicle.m_iPassengerCompromiseCount);
		if (vehicle.m_iLastVehicleHeatChangedSecond <= 0)
			vehicle.m_iLastVehicleHeatChangedSecond = vehicle.m_iStoredAtSecond;
		if (vehicle.m_bReported && vehicle.m_iVehicleHeat <= 0)
			vehicle.m_iVehicleHeat = 1;
		if (vehicle.m_bReported && vehicle.m_iLastReportedSecond <= 0)
			vehicle.m_iLastReportedSecond = vehicle.m_iLastVehicleHeatChangedSecond;
		if (vehicle.m_bReported && vehicle.m_sLastReportedReason.IsEmpty())
			vehicle.m_sLastReportedReason = "legacy/backfilled";
	}

	static void NormalizeRuntimeVehicleCoverState(HST_RuntimeVehicleState vehicle)
	{
		if (!vehicle)
			return;

		vehicle.m_iVehicleHeat = Math.Max(0, Math.Min(10, vehicle.m_iVehicleHeat));
		vehicle.m_iPassengerCompromiseCount = Math.Max(0, vehicle.m_iPassengerCompromiseCount);
		if (vehicle.m_iLastVehicleHeatChangedSecond <= 0)
			vehicle.m_iLastVehicleHeatChangedSecond = vehicle.m_iSpawnedAtSecond;
		if (vehicle.m_bReported && vehicle.m_iVehicleHeat <= 0)
			vehicle.m_iVehicleHeat = 1;
		if (vehicle.m_bReported && vehicle.m_iLastReportedSecond <= 0)
			vehicle.m_iLastReportedSecond = vehicle.m_iLastVehicleHeatChangedSecond;
		if (vehicle.m_bReported && vehicle.m_sLastReportedReason.IsEmpty())
			vehicle.m_sLastReportedReason = "legacy/backfilled";
	}
}
