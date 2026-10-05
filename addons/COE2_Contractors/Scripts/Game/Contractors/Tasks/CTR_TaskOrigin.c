//! Remembers which builder spawned a task: the builder class and task prefab are the reward type
//! (several builders share a task class or prefab, so neither identifies the task type alone).
modded class KSC_BaseTask
{
	protected string m_sCTR_BuilderClass;
	protected ResourceName m_sCTR_TaskPrefab;
	protected string m_sCTR_TaskName;

	//------------------------------------------------------------------------------------------------
	void CTR_SetBuilder(notnull KSC_BaseTaskBuilder builder)
	{
		m_sCTR_BuilderClass = builder.ClassName();
		m_sCTR_TaskPrefab = builder.GetTaskResourceName();

		COE_BaseTaskBuilder coeBuilder = COE_BaseTaskBuilder.Cast(builder);
		if (coeBuilder)
			m_sCTR_TaskName = coeBuilder.GetTaskName();
	}

	//------------------------------------------------------------------------------------------------
	string CTR_GetBuilderClass()
	{
		return m_sCTR_BuilderClass;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName CTR_GetTaskPrefab()
	{
		return m_sCTR_TaskPrefab;
	}

	//------------------------------------------------------------------------------------------------
	string CTR_GetTaskName()
	{
		return m_sCTR_TaskName;
	}
}

//------------------------------------------------------------------------------------------------
modded class KSC_BaseTaskBuilder
{
	//------------------------------------------------------------------------------------------------
	//! Every COE2 builder spawns its task entity through here.
	override protected KSC_BaseTask SpawnTaskEntity(vector pos)
	{
		KSC_BaseTask task = super.SpawnTaskEntity(pos);
		if (task)
			task.CTR_SetBuilder(this);

		return task;
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_AO
{
	//------------------------------------------------------------------------------------------------
	void CTR_GetTasks(notnull array<KSC_BaseTask> outTasks)
	{
		foreach (KSC_BaseTask task : m_aTasks)
		{
			if (task)
				outTasks.Insert(task);
		}
	}

	//------------------------------------------------------------------------------------------------
	string CTR_GetLocationName()
	{
		if (!m_Params)
			return string.Empty;

		KSC_Location location = m_Params.GetLocation();
		if (!location)
			return string.Empty;

		return location.m_sName;
	}
}
