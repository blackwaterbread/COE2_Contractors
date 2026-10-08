//! Where the exfil point may be relative to the AOs.
enum CTR_EExfilDistance
{
	OK,
	TOO_CLOSE,
	TOO_FAR
}

//------------------------------------------------------------------------------------------------
//! Exfil rules, without engine dependencies.
class CTR_ExfilRules
{
	//------------------------------------------------------------------------------------------------
	//! Players needed at the exfil point out of those alive outside the base.
	static int GetNeeded(int outside, float ratio)
	{
		if (outside <= 0)
			return 0;

		// Rounded first: 4 x 0.75 must stay 3 even when the product comes out as 3.0000001.
		float needed = Math.Round(outside * ratio * 1000) / 1000;
		return Math.Max(1, Math.Ceil(needed));
	}

	//------------------------------------------------------------------------------------------------
	//! Enough players are at the exfil point. Nobody outside the base cannot exfil.
	static bool IsMet(int present, int outside, float ratio)
	{
		return present > 0 && present >= GetNeeded(outside, ratio);
	}

	//------------------------------------------------------------------------------------------------
	//! Chance of an enemy pursuit after this many civilians were killed inside an AO.
	static float GetPursuitChance(float baseChance, float perCivilian, int civilians)
	{
		return Math.Clamp(baseChance + perCivilian * Math.Max(0, civilians), 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Pursuers in one wave for this many living players outside the base.
	static int GetWaveSize(int outside, float perPlayer, int min, int max)
	{
		return Math.Clamp(Math.Round(outside * perPlayer), min, Math.Max(min, max));
	}

	//------------------------------------------------------------------------------------------------
	//! Distance in the XZ plane from the edge of a circular AO; negative inside it.
	static float GetEdgeDistance(vector pos, vector center, float radius)
	{
		return vector.DistanceXZ(pos, center) - radius;
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil point must be at least minDistance from the edge of every AO and at most maxDistance from the edge of
	//! the nearest one (0 = no limit). Without AOs anything goes.
	static CTR_EExfilDistance CheckDistance(vector pos, notnull array<vector> centers, float radius, float minDistance, float maxDistance)
	{
		if (centers.IsEmpty())
			return CTR_EExfilDistance.OK;

		float nearest = float.MAX;
		foreach (vector center : centers)
		{
			nearest = Math.Min(nearest, GetEdgeDistance(pos, center, radius));
		}

		if (nearest < minDistance)
			return CTR_EExfilDistance.TOO_CLOSE;

		if (maxDistance > 0 && nearest > maxDistance)
			return CTR_EExfilDistance.TOO_FAR;

		return CTR_EExfilDistance.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! One of eight compass directions (0 = north, clockwise) from one point to another in the XZ plane.
	static int GetCompassOctant(vector from, vector to)
	{
		vector dir = to - from;
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		if (yaw < 0)
			yaw += 360;

		int octant = Math.Round(yaw / 45);
		return octant % 8;
	}
}
