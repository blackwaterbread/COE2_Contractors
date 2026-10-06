//! Lets operations see whom a CPR performer is working on (ACE Medical Circulation keeps the patient protected).
modded class ACE_Medical_CPRHelperCompartment
{
	//------------------------------------------------------------------------------------------------
	ACE_Medical_VitalsComponent CTR_GetPatientVitals()
	{
		return m_pPatientVitals;
	}
}
