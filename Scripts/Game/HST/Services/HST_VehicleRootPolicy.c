class HST_VehicleRootPolicy
{
	static bool IsEligibleVehicleRootPrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		if (!prefab.Contains("Prefabs/"))
			return false;

		if (IsRejectedVehicleRootPrefab(prefab))
			return false;

		if (IsVehiclePartPrefab(prefab))
			return false;

		if (prefab.Contains("Prefabs/Vehicles/"))
			return true;

		return prefab.Contains("Vehicle") || prefab.Contains("vehicle");
	}

	static bool IsRejectedVehicleRootPrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return true;

		if (prefab.Contains("Supply") || prefab.Contains("supply"))
			return true;

		if (prefab.Contains("Crate") || prefab.Contains("crate") || prefab.Contains("Cache") || prefab.Contains("cache"))
			return true;

		if (prefab.Contains("Box") || prefab.Contains("box") || prefab.Contains("Cargo") || prefab.Contains("cargo") || prefab.Contains("Container") || prefab.Contains("container"))
			return true;

		if (prefab.Contains("Arsenal") || prefab.Contains("arsenal") || prefab.Contains("Composition") || prefab.Contains("composition") || prefab.Contains("Props/") || prefab.Contains("/Props"))
			return true;

		return false;
	}

	static bool IsVehiclePartPrefab(string prefab)
	{
		return IsVehiclePartName(prefab);
	}

	static bool IsVehiclePartName(string name)
	{
		if (name.IsEmpty())
			return false;

		if (name.Contains("/VehParts/") || name.Contains("VehParts/") || name.Contains("VehicleParts"))
			return true;

		if (name.Contains("LicensePlate") || name.Contains("licenseplate") || name.Contains("License_Plate"))
			return true;

		if (name.Contains("Wheel") || name.Contains("wheel") || name.Contains("Tire") || name.Contains("tire") || name.Contains("Tyre") || name.Contains("tyre"))
			return true;

		if (name.Contains("Suspension") || name.Contains("suspension") || name.Contains("Axle") || name.Contains("axle"))
			return true;

		if (name.Contains("Turret") || name.Contains("turret") || name.Contains("Rotor") || name.Contains("rotor") || name.Contains("Door") || name.Contains("door") || name.Contains("Seat") || name.Contains("seat"))
			return true;

		if (name.Contains("Mirror") || name.Contains("mirror") || name.Contains("Window") || name.Contains("window") || name.Contains("Headlight") || name.Contains("headlight"))
			return true;

		if (name.Contains("Light") || name.Contains("light") || name.Contains("Attachment") || name.Contains("attachment") || name.Contains("Proxy") || name.Contains("proxy") || name.Contains("Damage") || name.Contains("damage"))
			return true;

		return false;
	}

	static string BuildRejectReason(IEntity entity, string prefab)
	{
		if (!entity)
			return "candidate missing";

		if (prefab.IsEmpty())
			return "candidate had no prefab";

		if (IsRejectedVehicleRootPrefab(prefab))
			return "prefab is cargo/supply/arsenal/prop, not vehicle root";

		if (IsVehiclePartPrefab(prefab) || IsVehiclePartName(entity.GetName()))
			return "candidate is a vehicle part, license plate, seat, turret, wheel, or proxy";

		if (!IsEligibleVehicleRootPrefab(prefab))
			return "prefab is not an eligible vehicle root resource";

		return "candidate passed prefab checks";
	}
}
