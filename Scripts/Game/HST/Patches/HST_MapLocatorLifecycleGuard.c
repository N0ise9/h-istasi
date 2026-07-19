modded class SCR_MapLocator
{
	override protected void CalculateClosestLocation()
	{
		if (!m_wUIHintLayout || !m_wUIHintText || !m_wUIHintText2 || !m_WorldDirections)
		{
			if (m_wUIHintLayout)
				m_wUIHintLayout.RemoveFromHierarchy();

			m_wUIHintLayout = null;
			m_wUIHintText = null;
			m_wUIHintText2 = null;
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(CalculateClosestLocation);
			return;
		}

		super.CalculateClosestLocation();
	}
}
