class HST_ForcePlanningIntegrityService
{
	static const string GARRISON_POLICY_ID = "garrison_exact_all_or_nothing_1";
	protected ref HST_ForceCatalogService m_Catalog = new HST_ForceCatalogService();

	string BuildManifestHash(HST_ForceManifestState manifest)
	{
		if (!manifest)
			return "";
		string canonical = string.Format(
			"%1|%2|%3|%4|%5|%6|%7|%8|%9",
			manifest.m_sManifestId,
			manifest.m_sOperationId,
			manifest.m_sQuoteId,
			manifest.m_sCommandRequestId,
			manifest.m_sForceKind,
			manifest.m_sFactionRole,
			manifest.m_sFactionKey,
			manifest.m_sIntentId,
			manifest.m_sSourceZoneId
		);
		canonical = canonical + string.Format(
			"|%1|%2|%3|%4|%5|%6|%7|%8|%9",
			manifest.m_sTargetZoneId,
			manifest.m_sGroupPrefab,
			manifest.m_sCatalogVersion,
			manifest.m_sPolicyId,
			manifest.m_iRequestedMemberCount,
			manifest.m_iAcceptedMemberCount,
			manifest.m_iRequestedVehicleCount,
			manifest.m_iAcceptedVehicleCount,
			manifest.m_iMoneyCost
		);
		canonical = canonical + string.Format(
			"|%1|%2|%3|%4|%5|%6|%7",
			manifest.m_iHRCost,
			manifest.m_iEquipmentCost,
			manifest.m_iAttackResourceCost,
			manifest.m_iSupportResourceCost,
			manifest.m_iDeterministicSeed,
			manifest.m_iCreatedAtSecond,
			manifest.m_bFrozen
		);
		foreach (HST_ForceManifestGroupState group : manifest.m_aGroups)
		{
			if (!group)
				continue;
			canonical = canonical + string.Format("|g:%1:%2:%3:%4:%5:%6:%7", group.m_sElementId, group.m_sCatalogEntryId, group.m_sPrefab, group.m_sRole, group.m_iOrdinal, group.m_iExpectedMemberCount, group.m_bRequired);
		}
		foreach (HST_ForceManifestMemberState member : manifest.m_aMembers)
		{
			if (!member)
				continue;
			canonical = canonical + string.Format("|m:%1:%2:%3:%4:%5:%6:%7:%8:%9", member.m_sSlotId, member.m_sCatalogSlotId, member.m_sGroupElementId, member.m_sPrefab, member.m_sRole, member.m_sAssignedVehicleSlotId, member.m_sSeatRole, member.m_iSeatIndex, member.m_iOrdinal);
			canonical = canonical + string.Format(":%1:%2:%3:%4", member.m_iMoneyCost, member.m_iHRCost, member.m_iEquipmentCost, member.m_bRequired);
		}
		foreach (HST_ForceManifestVehicleState vehicle : manifest.m_aVehicles)
		{
			if (!vehicle)
				continue;
			canonical = canonical + string.Format("|v:%1:%2:%3:%4:%5:%6:%7:%8:%9", vehicle.m_sSlotId, vehicle.m_sCatalogEntryId, vehicle.m_sGroupElementId, vehicle.m_sPrefab, vehicle.m_sRole, vehicle.m_iOrdinal, vehicle.m_iMoneyCost, vehicle.m_iRequiredCrew, vehicle.m_bArmed);
			canonical = canonical + string.Format(":%1:%2:%3", vehicle.m_bLightArmor, vehicle.m_bHeavyArmor, vehicle.m_bRequired);
		}
		foreach (HST_ForceManifestAssetState asset : manifest.m_aAssets)
		{
			if (!asset)
				continue;
			canonical = canonical + string.Format("|a:%1:%2:%3:%4:%5:%6:%7:%8", asset.m_sSlotId, asset.m_sKind, asset.m_sPrefab, asset.m_sRole, asset.m_sAssignedVehicleSlotId, asset.m_iQuantity, asset.m_iOrdinal, asset.m_bRequired);
		}
		return string.Format("fm1_%1_%2", canonical.Hash(), (canonical + "|secondary").Hash());
	}

	string BuildGarrisonContextHash(HST_CampaignState state, HST_ZoneState zone, string factionKey)
	{
		if (!state || !zone)
			return "";
		int abstractInfantry;
		HST_GarrisonState garrison = state.FindGarrison(zone.m_sZoneId, factionKey);
		if (garrison)
			abstractInfantry = Math.Max(0, garrison.m_iInfantryCount);
		string canonical = string.Format("%1|%2|%3|%4|%5|%6|%7|%8", zone.m_sZoneId, zone.m_sOwnerFactionKey, factionKey, zone.m_eType, zone.m_iGarrisonSlots, abstractInfantry, Math.Max(0, zone.m_iActiveInfantryCount), HST_ForceCatalogService.CATALOG_VERSION);
		return string.Format("gc1_%1", canonical.Hash());
	}

	HST_ForceMemberCatalogEntry SelectGarrisonMember(array<ref HST_ForceMemberCatalogEntry> catalog, int seed, int memberIndex)
	{
		if (!catalog || catalog.Count() == 0)
			return null;
		if (memberIndex == 0)
			return catalog[0];
		int selectedIndex = PositiveModulo(seed + memberIndex * 31, catalog.Count());
		return catalog[selectedIndex];
	}

	int BuildDeterministicSeed(HST_CampaignState state, string requestIdentity, string zoneId)
	{
		return string.Format("%1|force_planning|%2|%3|%4", state.m_iCampaignSeed, requestIdentity, zoneId, HST_ForceCatalogService.CATALOG_VERSION).Hash();
	}

	bool ValidateFrozenGarrisonQuote(HST_ForceManifestState manifest, HST_ForceQuoteState quote, bool requireCurrentCatalog, out string failure)
	{
		failure = "";
		if (!manifest || !quote)
		{
			failure = "quote or manifest missing";
			return false;
		}
		if (requireCurrentCatalog)
		{
			HST_ForceCatalogValidationResult catalogValidation = m_Catalog.ValidateMemberCatalog(quote.m_sFactionKey, true);
			if (!catalogValidation || !catalogValidation.m_bValid)
			{
				failure = "force member catalog no longer validates";
				if (catalogValidation && !catalogValidation.m_sFailureReason.IsEmpty())
					failure = catalogValidation.m_sFailureReason;
				return false;
			}
		}
		if (!manifest.m_bFrozen)
		{
			failure = "manifest is not frozen";
			return false;
		}
		if (manifest.m_sManifestHash != quote.m_sManifestHash || BuildManifestHash(manifest) != quote.m_sManifestHash)
		{
			failure = "manifest hash conflict";
			return false;
		}
		if (manifest.m_sCatalogVersion.IsEmpty() || manifest.m_sCatalogVersion != quote.m_sCatalogVersion)
		{
			failure = "manifest catalog conflict";
			return false;
		}
		if (requireCurrentCatalog && manifest.m_sCatalogVersion != HST_ForceCatalogService.CATALOG_VERSION)
		{
			failure = "quote catalog version is no longer current";
			return false;
		}
		return ValidateGarrisonManifest(manifest, quote, requireCurrentCatalog, failure);
	}

	bool ValidateGarrisonManifest(HST_ForceManifestState manifest, HST_ForceQuoteState quote, bool requireCurrentCatalog, out string failure)
	{
		failure = "";
		if (!manifest || !quote)
		{
			failure = "quote or manifest missing";
			return false;
		}
		if (!ValidateManifestIdentity(manifest, quote))
		{
			failure = "quote and manifest identity conflict";
			return false;
		}
		if (!ValidateManifestPolicy(manifest, quote, requireCurrentCatalog))
		{
			failure = "garrison policy or catalog conflict";
			return false;
		}
		if (!ValidateManifestCounts(manifest, quote))
		{
			failure = "quote and manifest force totals conflict";
			return false;
		}
		if (manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0] || manifest.m_aGroups[0].m_iExpectedMemberCount != manifest.m_iAcceptedMemberCount)
		{
			failure = "garrison group element conflict";
			return false;
		}

		array<ref HST_ForceMemberCatalogEntry> memberCatalog = {};
		if (requireCurrentCatalog)
			memberCatalog = m_Catalog.BuildMemberCatalog(manifest.m_sFactionKey);
		array<string> slotIds = {};
		int moneyCost;
		int hrCost;
		int equipmentCost;
		foreach (HST_ForceManifestMemberState member : manifest.m_aMembers)
		{
			if (!ValidateManifestMember(member, manifest.m_aGroups[0], slotIds, memberCatalog, requireCurrentCatalog))
			{
				failure = "garrison member slot or catalog conflict";
				return false;
			}
			slotIds.Insert(member.m_sSlotId);
			moneyCost += member.m_iMoneyCost;
			hrCost += member.m_iHRCost;
			equipmentCost += member.m_iEquipmentCost;
		}
		if (moneyCost != manifest.m_iMoneyCost || hrCost != manifest.m_iHRCost || equipmentCost != manifest.m_iEquipmentCost)
		{
			failure = "manifest resource totals conflict";
			return false;
		}
		if (manifest.m_iMoneyCost != quote.m_iMoneyCost || manifest.m_iHRCost != quote.m_iHRCost || manifest.m_iEquipmentCost != quote.m_iEquipmentCost)
		{
			failure = "quote resource totals conflict";
			return false;
		}
		if (manifest.m_iEquipmentCost != 0 || manifest.m_iAttackResourceCost != 0 || manifest.m_iSupportResourceCost != 0 || quote.m_iAttackResourceCost != 0 || quote.m_iSupportResourceCost != 0)
		{
			failure = "unsupported garrison resource cost";
			return false;
		}
		return true;
	}

	bool TransactionMatchesQuote(HST_ResourceTransactionState transaction, HST_ForceQuoteState quote, string resourceType, int amount)
	{
		if (!TransactionHasQuoteIdentity(transaction, quote, resourceType, amount))
			return false;
		if (quote.m_sConfirmationRequestId.IsEmpty() || transaction.m_sCommandRequestId != quote.m_sConfirmationRequestId)
			return false;
		return transaction.m_iRefundedAmount == 0 && transaction.m_eStatus == HST_EResourceTransactionStatus.HST_TRANSACTION_COMMITTED;
	}

	bool ReservationMatchesQuote(HST_ResourceTransactionState transaction, HST_ForceQuoteState quote, string resourceType, int amount, string confirmationRequestId)
	{
		if (!TransactionHasQuoteIdentity(transaction, quote, resourceType, amount))
			return false;
		if (confirmationRequestId.IsEmpty() || transaction.m_sCommandRequestId != confirmationRequestId)
			return false;
		return transaction.m_iRefundedAmount == 0 && transaction.m_eStatus == HST_EResourceTransactionStatus.HST_TRANSACTION_RESERVED;
	}

	bool TransactionHasQuoteIdentity(HST_ResourceTransactionState transaction, HST_ForceQuoteState quote, string resourceType, int amount)
	{
		if (!transaction || !quote)
			return false;
		string expectedTransactionId = quote.m_sMoneyTransactionId;
		if (resourceType == HST_ResourceLedgerService.RESOURCE_HR)
			expectedTransactionId = quote.m_sHRTransactionId;
		if (transaction.m_sTransactionId != expectedTransactionId || transaction.m_sQuoteId != quote.m_sQuoteId)
			return false;
		if (transaction.m_sManifestId != quote.m_sManifestId || transaction.m_sOperationId != quote.m_sOperationId)
			return false;
		if (transaction.m_sActorIdentityId != quote.m_sActorIdentityId || transaction.m_sResourceType != resourceType)
			return false;
		return transaction.m_iAmount == amount;
	}

	protected bool ValidateManifestIdentity(HST_ForceManifestState manifest, HST_ForceQuoteState quote)
	{
		if (manifest.m_sQuoteId != quote.m_sQuoteId || manifest.m_sOperationId != quote.m_sOperationId)
			return false;
		if (manifest.m_sCommandRequestId != quote.m_sCommandRequestId || manifest.m_sFactionKey != quote.m_sFactionKey)
			return false;
		return manifest.m_sTargetZoneId == quote.m_sTargetZoneId;
	}

	protected bool ValidateManifestPolicy(HST_ForceManifestState manifest, HST_ForceQuoteState quote, bool requireCurrentCatalog)
	{
		if (manifest.m_sForceKind != "strategic_garrison" || manifest.m_sIntentId != "garrison_recruitment")
			return false;
		if (manifest.m_sPolicyId.IsEmpty() || quote.m_sPolicyId != manifest.m_sPolicyId)
			return false;
		if (quote.m_sCatalogVersion != manifest.m_sCatalogVersion || !quote.m_bAllOrNothing)
			return false;
		return !requireCurrentCatalog || manifest.m_sPolicyId == GARRISON_POLICY_ID;
	}

	protected bool ValidateManifestCounts(HST_ForceManifestState manifest, HST_ForceQuoteState quote)
	{
		if (manifest.m_iRequestedMemberCount <= 0 || manifest.m_iRequestedMemberCount != manifest.m_iAcceptedMemberCount)
			return false;
		if (manifest.m_iAcceptedMemberCount != manifest.m_aMembers.Count())
			return false;
		if (manifest.m_iRequestedMemberCount != quote.m_iRequestedMemberCount || manifest.m_iAcceptedMemberCount != quote.m_iAcceptedMemberCount)
			return false;
		if (manifest.m_iRequestedVehicleCount != 0 || manifest.m_iAcceptedVehicleCount != 0)
			return false;
		if (manifest.m_aVehicles.Count() != 0 || manifest.m_aAssets.Count() != 0)
			return false;
		return quote.m_iRequestedVehicleCount == 0 && quote.m_iAcceptedVehicleCount == 0;
	}

	protected bool ValidateManifestMember(HST_ForceManifestMemberState member, HST_ForceManifestGroupState group, array<string> slotIds, array<ref HST_ForceMemberCatalogEntry> memberCatalog, bool requireCurrentCatalog)
	{
		if (!member || !group || member.m_sSlotId.IsEmpty() || slotIds.Contains(member.m_sSlotId))
			return false;
		if (member.m_sGroupElementId != group.m_sElementId || !member.m_bRequired)
			return false;
		if (member.m_sCatalogSlotId.IsEmpty() || member.m_sPrefab.IsEmpty() || member.m_sRole.IsEmpty())
			return false;
		if (member.m_iMoneyCost < 0 || member.m_iHRCost < 0 || member.m_iEquipmentCost < 0)
			return false;
		if (!requireCurrentCatalog)
			return true;
		HST_ForceMemberCatalogEntry catalogEntry = FindMemberCatalogEntry(memberCatalog, member.m_sCatalogSlotId);
		if (!catalogEntry || catalogEntry.m_sPrefab != member.m_sPrefab || catalogEntry.m_sRole != member.m_sRole)
			return false;
		return catalogEntry.m_iMoneyCost == member.m_iMoneyCost && catalogEntry.m_iHRCost == member.m_iHRCost && catalogEntry.m_iEquipmentCost == member.m_iEquipmentCost;
	}

	protected HST_ForceMemberCatalogEntry FindMemberCatalogEntry(array<ref HST_ForceMemberCatalogEntry> catalog, string entryId)
	{
		foreach (HST_ForceMemberCatalogEntry entry : catalog)
		{
			if (entry && entry.m_sEntryId == entryId)
				return entry;
		}
		return null;
	}

	protected int PositiveModulo(int value, int divisor)
	{
		if (divisor <= 0)
			return 0;
		int result = value % divisor;
		if (result < 0)
			result += divisor;
		return result;
	}
}
