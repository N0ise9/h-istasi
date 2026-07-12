class HST_LocalSecurityCatalogResult
{
	bool m_bSuccess;
	string m_sFailureReason;
	ref HST_ForceGroupCatalogEntry m_Group;
}

// Dedicated authored roster reader for automatic town security. Keeping this
// catalog separate prevents police groups from entering the generic combat-force
// selection pools.
class HST_LocalSecurityCatalogService
{
	static const string CATALOG_VERSION = "local_security_catalog_1";
	static const string ROLE_TOWN_POLICE = "town_police";
	static const int MIN_MEMBER_COUNT = 2;
	static const int MAX_MEMBER_COUNT = 5;

	HST_LocalSecurityCatalogResult ResolveAuthoredGroup(
		string factionKey,
		int policeStrength)
	{
		HST_LocalSecurityCatalogResult result = new HST_LocalSecurityCatalogResult();
		factionKey = factionKey.Trim();
		if (factionKey.IsEmpty())
		{
			result.m_sFailureReason = "local-security catalog requires a faction";
			return result;
		}

		int requestedCount = ResolveMemberCount(policeStrength);
		ResourceName groupPrefab = HST_DefaultCatalog.ResolveTownPoliceGroupPrefab(
			factionKey,
			requestedCount);
		if (groupPrefab.IsEmpty())
		{
			result.m_sFailureReason = "local-security faction has no authored town-police group";
			return result;
		}

		array<ResourceName> authoredSlots;
		string failure;
		if (!ReadAuthoredSlots(groupPrefab, requestedCount, authoredSlots, failure))
		{
			result.m_sFailureReason = failure;
			return result;
		}

		HST_ForceGroupCatalogEntry group = new HST_ForceGroupCatalogEntry();
		group.m_sEntryId = string.Format(
			"local_security_%1_%2",
			factionKey,
			requestedCount);
		group.m_sFactionKey = factionKey;
		group.m_sRole = ROLE_TOWN_POLICE;
		group.m_sAuthoredPrefab = groupPrefab;
		group.m_sExecutionPrefab = groupPrefab;
		for (int slotIndex = 0; slotIndex < authoredSlots.Count(); slotIndex++)
		{
			HST_ForceGroupCatalogSlot slot = new HST_ForceGroupCatalogSlot();
			slot.m_sSlotId = string.Format("town_police_%1", slotIndex + 1);
			slot.m_sPrefab = authoredSlots[slotIndex];
			slot.m_sRole = ROLE_TOWN_POLICE;
			slot.m_iOrdinal = slotIndex;
			slot.m_bRequired = true;
			group.m_aMemberSlots.Insert(slot);
		}

		result.m_bSuccess = true;
		result.m_Group = group;
		return result;
	}

	static int ResolveMemberCount(int policeStrength)
	{
		return Math.Max(
			MIN_MEMBER_COUNT,
			Math.Min(MAX_MEMBER_COUNT, Math.Max(0, policeStrength) + 1));
	}

	protected bool ReadAuthoredSlots(
		ResourceName groupPrefab,
		int expectedCount,
		out array<ResourceName> slots,
		out string failure)
	{
		slots = {};
		failure = "";
		if (expectedCount < MIN_MEMBER_COUNT || expectedCount > MAX_MEMBER_COUNT)
		{
			failure = "local-security authored roster count is outside 2..5";
			return false;
		}

		Resource groupResource = Resource.Load(groupPrefab);
		if (!groupResource || !groupResource.IsValid())
		{
			failure = "local-security authored group resource is missing or invalid";
			return false;
		}
		IEntitySource groupSource = SCR_BaseContainerTools.FindEntitySource(groupResource);
		typename groupType;
		if (groupSource)
			groupType = groupSource.GetClassName().ToType();
		if (!groupSource || !groupType || !groupType.IsInherited(SCR_AIGroup))
		{
			failure = "local-security authored group is not an SCR_AIGroup";
			return false;
		}
		if (!groupSource.Get("m_aUnitPrefabSlots", slots) || !slots)
		{
			failure = "local-security authored group has no readable member slots";
			return false;
		}
		if (slots.Count() != expectedCount)
		{
			failure = string.Format(
				"local-security authored roster count %1 differs from expected %2",
				slots.Count(),
				expectedCount);
			return false;
		}

		for (int slotIndex = 0; slotIndex < slots.Count(); slotIndex++)
		{
			ResourceName memberPrefab = slots[slotIndex];
			Resource memberResource = Resource.Load(memberPrefab);
			IEntitySource memberSource;
			typename memberType;
			if (memberResource && memberResource.IsValid())
				memberSource = SCR_BaseContainerTools.FindEntitySource(memberResource);
			if (memberSource)
				memberType = memberSource.GetClassName().ToType();
			if (memberPrefab.IsEmpty() || !memberResource || !memberResource.IsValid()
				|| !memberSource || !memberType
				|| !memberType.IsInherited(SCR_ChimeraCharacter))
			{
				failure = string.Format(
					"local-security authored member slot %1 is invalid",
					slotIndex);
				return false;
			}
		}
		return true;
	}
}
