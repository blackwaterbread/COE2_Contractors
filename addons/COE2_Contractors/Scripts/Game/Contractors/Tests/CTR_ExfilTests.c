#ifdef WORKBENCH
class CTR_ExfilTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_ExfilNeeded());
		runner.Add(new CTR_Test_ExfilDistance());
		runner.Add(new CTR_Test_PursuitRules());
	}
}

//------------------------------------------------------------------------------------------------
//! Players needed at the exfil point: 75% of the living players outside the base, rounded up.
class CTR_Test_ExfilNeeded : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		array<int> outside = {1, 2, 3, 4, 5, 6, 8};
		array<int> needed = {1, 2, 3, 3, 4, 5, 6};
		foreach (int i, int count : outside)
		{
			CheckInt(CTR_ExfilRules.GetNeeded(count, 0.75), needed[i], string.Format("needed of %1", count));
		}

		CheckInt(CTR_ExfilRules.GetNeeded(0, 0.75), 0, "nobody outside");
		CheckInt(CTR_ExfilRules.GetNeeded(10, 0.01), 1, "at least one");
		Check(CTR_ExfilRules.IsMet(3, 4, 0.75), "3 of 4");
		Check(!CTR_ExfilRules.IsMet(2, 4, 0.75), "2 of 4");
		Check(CTR_ExfilRules.IsMet(1, 1, 0.75), "alone");
		Check(!CTR_ExfilRules.IsMet(0, 0, 0.75), "nobody outside cannot exfil");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The exfil point must be 1 km from the edge of every AO and at most 2 km from the edge of the nearest one.
class CTR_Test_ExfilDistance : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		array<vector> one = {"0 0 0"};
		float radius = 250;
		CheckInt(CTR_ExfilRules.CheckDistance("1000 0 0", one, radius, 1000, 2000), CTR_EExfilDistance.TOO_CLOSE, "750 m from the edge");
		CheckInt(CTR_ExfilRules.CheckDistance("1250 0 0", one, radius, 1000, 2000), CTR_EExfilDistance.OK, "1 km from the edge");
		CheckInt(CTR_ExfilRules.CheckDistance("0 0 2250", one, radius, 1000, 2000), CTR_EExfilDistance.OK, "2 km from the edge");
		CheckInt(CTR_ExfilRules.CheckDistance("0 0 2300", one, radius, 1000, 2000), CTR_EExfilDistance.TOO_FAR, "2.05 km from the edge");
		CheckInt(CTR_ExfilRules.CheckDistance("0 0 5000", one, radius, 1000, 0), CTR_EExfilDistance.OK, "no limit");
		CheckInt(CTR_ExfilRules.CheckDistance("0 500 1500", one, radius, 1000, 2000), CTR_EExfilDistance.OK, "height does not count");

		// Two AOs: far enough from both, near enough to one.
		array<vector> two = {"0 0 0", "3000 0 0"};
		CheckInt(CTR_ExfilRules.CheckDistance("2000 0 0", two, radius, 1000, 2000), CTR_EExfilDistance.TOO_CLOSE, "too close to the second");
		CheckInt(CTR_ExfilRules.CheckDistance("-1500 0 0", two, radius, 1000, 2000), CTR_EExfilDistance.OK, "near the first, far from the second");
		CheckInt(CTR_ExfilRules.CheckDistance("1500 0 1500", two, radius, 1000, 2000), CTR_EExfilDistance.OK, "1.87 km from both edges");

		array<vector> none = {};
		CheckInt(CTR_ExfilRules.CheckDistance("0 0 0", none, radius, 1000, 2000), CTR_EExfilDistance.OK, "no AO picked yet");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Pursuit chance by civilians killed, wave size by players outside the base, compass directions.
class CTR_Test_PursuitRules : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		array<float> chances = {0.17, 0.34, 0.51, 0.68, 0.85, 1, 1};
		foreach (int civilians, float chance : chances)
		{
			Check(Math.AbsFloat(CTR_ExfilRules.GetPursuitChance(0.17, 0.17, civilians) - chance) < 0.001, string.Format("chance after %1 civilians", civilians));
		}

		CheckInt(CTR_ExfilRules.GetWaveSize(1, 1.5, 8, 16), 8, "one player: at least 8");
		CheckInt(CTR_ExfilRules.GetWaveSize(6, 1.5, 8, 16), 9, "six players");
		CheckInt(CTR_ExfilRules.GetWaveSize(20, 1.5, 8, 16), 16, "at most 16");

		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "0 0 100"), 0, "north");
		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "100 0 100"), 1, "north-east");
		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "100 0 0"), 2, "east");
		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "0 0 -100"), 4, "south");
		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "-100 0 0"), 6, "west");
		CheckInt(CTR_ExfilRules.GetCompassOctant("0 0 0", "-10 0 100"), 0, "almost north");
		Finish();
	}
}
#endif
