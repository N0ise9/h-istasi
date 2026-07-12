modded class SCR_MapMarkerManagerComponent
{
	void HST_InsertProtectedLocalStaticMarker(SCR_MapMarkerBase marker)
	{
		if (!marker)
			return;

		marker.SetMarkerID(-1);
		marker.SetMarkerOwnerID(-1);
		marker.SetCanBeRemovedByOwner(false);
		m_aStaticMarkers.Insert(marker);
		marker.OnCreateMarker(true);
	}

	override void Update(float timeSlice)
	{
		if (!m_MapEntity)
			return;

		m_MapEntity.GetMapVisibleFrame(m_vVisibleFrameMin, m_vVisibleFrameMax);
		for (int i = m_aStaticMarkers.Count() - 1; i >= 0; i--)
		{
			SCR_MapMarkerBase marker = m_aStaticMarkers[i];
			if (!marker)
			{
				m_aStaticMarkers.Remove(i);
				continue;
			}

			if (!marker.GetRootWidget())
			{
				marker.OnCreateMarker(true);
				if (!marker.GetRootWidget())
				{
					SetStaticMarkerDisabled(marker, true);
					continue;
				}
			}

			if (!marker.OnUpdate(m_vVisibleFrameMin, m_vVisibleFrameMax))
				SetStaticMarkerDisabled(marker, true);
			else
				SetStaticMarkerDisabled(marker, false);
		}

		foreach (SCR_MapMarkerEntity markerEntity : m_aDynamicMarkers)
		{
			if (markerEntity)
				markerEntity.OnUpdate();
		}

		#ifdef MARKERS_DEBUG
			UpdateDebug(timeSlice);
		#endif
	}
}
