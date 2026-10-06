#ifdef WORKBENCH
//! Workbench only: runs commands from $profile:ctr_cmd.txt (one line, the file is deleted), for automation that cannot
//! type in the game view. Every "#ctr" action works without the "#ctr" ("ao 1", "win", "cash 5000"), plus:
//!   gear       log the host's carried items (prefab, issued, rounds)
//!   inv        open the inventory
//!   qm         stand in front of the quartermaster, facing him
//!   shop       open the quartermaster's shop window
//!   stash      open the stash at the base
//!   close      close the menus
//!   equip      take the first weapon in hand
//!   out        move 120 m away from the base, out of the safe zone
//!   hands      log what the host holds
//!   view       switch between first and third person
//!   kill       kill the host's character (to respawn)
//!   pause      open the pause menu
class CTR_DevFileCommands
{
	static const string FILE = "$profile:ctr_cmd.txt";
	protected static const int POLL_MS = 500;

	//------------------------------------------------------------------------------------------------
	static void Start()
	{
		Print("[CTR_DEV] waiting for commands in " + FILE);
		GetGame().GetCallqueue().Remove(Poll);
		GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Poll()
	{
		if (!FileIO.FileExists(FILE))
			return;

		FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
		string line;
		if (file)
		{
			file.ReadLine(line);
			file.Close();
		}

		FileIO.DeleteFile(FILE);
		line.Trim();
		array<string> words = {};
		line.Split(" ", words, true);
		if (words.IsEmpty())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		IEntity character = controller.GetControlledEntity();
		Print("[CTR_DEV] command: " + line);
		switch (words[0])
		{
			case "gear": LogGear(character); return;
			case "inv": OpenInventory(character); return;
			case "qm": FaceQuartermaster(controller.GetPlayerId()); return;
			case "shop": OpenShop(); return;
			case "stash": OpenStash(controller); return;
			case "close": GetGame().GetMenuManager().CloseAllMenus(); return;
			case "equip": EquipWeapon(character); return;
			case "out": MoveFromBase(controller.GetPlayerId()); return;
			case "hands": LogHands(character); return;
			case "view": ToggleView(character); return;
			case "kill": Kill(character); return;
			case "pause": ArmaReforgerScripted.OpenPauseMenu(); return;
		}

		array<string> argv = {CTR_DevCommand.KEYWORD};
		argv.InsertAll(words);
		CTR_DevCommand command = new CTR_DevCommand();
		ScrServerCmdResult result = command.OnChatServerExecution(argv, controller.GetPlayerId());
		if (result)
			Print("[CTR_DEV] " + result.m_sResponse);
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogGear(IEntity character)
	{
		if (!character)
			return;

		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(character, items);
		foreach (IEntity item : items)
		{
			string rounds;
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine)
				rounds = string.Format(" rounds %1/%2", magazine.GetAmmoCount(), magazine.GetMaxAmmoCount());

			Print(string.Format("[CTR_DEV] gear %1 issued=%2%3", FilePath.StripPath(SCR_ResourceNameUtils.GetPrefabName(item)), MRX_IssuedItems.IsIssued(item), rounds));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the first weapon (primary first) in hand.
	protected static void EquipWeapon(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		BaseWeaponManagerComponent weaponManager;
		if (chimera)
			weaponManager = BaseWeaponManagerComponent.Cast(chimera.FindComponent(BaseWeaponManagerComponent));

		if (!weaponManager)
			return;

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot.GetWeaponEntity())
				continue;

			chimera.GetCharacterController().TryEquipRightHandItem(slot.GetWeaponEntity(), EEquipItemType.EEquipTypeWeapon, false);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogHands(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		BaseWeaponManagerComponent weaponManager = controller.GetWeaponManagerComponent();
		string weapon;
		if (weaponManager && weaponManager.GetCurrentWeapon())
			weapon = FilePath.StripPath(SCR_ResourceNameUtils.GetPrefabName(weaponManager.GetCurrentWeapon().GetOwner()));

		string inHands;
		if (controller.GetCurrentItemInHands())
			inHands = FilePath.StripPath(SCR_ResourceNameUtils.GetPrefabName(controller.GetCurrentItemInHands()));

		Print(string.Format("[CTR_DEV] hands item=%1 weapon=%2 changing=%3 raised=%4 canFire=%5 third=%6", inHands, weapon, controller.IsChangingItem(), controller.IsWeaponRaised(), controller.GetCanFireWeapon(), controller.IsInThirdPersonView()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void ToggleView(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (chimera && chimera.GetCharacterController())
			chimera.GetCharacterController().SetInThirdPersonView(!chimera.GetCharacterController().IsInThirdPersonView());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Kill(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (chimera && chimera.GetCharacterController())
			chimera.GetCharacterController().ForceDeath();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the player 120 m from the main base, out of the safe zone.
	protected static void MoveFromBase(int playerId)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return;

		vector pos = gameMode.GetMainBasePos() + "120 0 0";
		vector free;
		if (SCR_WorldTools.FindEmptyTerrainPosition(free, pos, 20))
			pos = free;

		Print("[CTR_DEV] " + CTR_DevTools.Teleport(playerId, pos));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenInventory(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return;

		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(chimera.GetCharacterController().GetInventoryStorageManager());
		if (manager)
			manager.OpenInventory();
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_ShopComponent FindQuartermaster()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return null;

		IEntity entity = CTR_DevTools.FindNear(gameMode.GetMainBasePos(), 60, MRX_ShopKeeperComponent);
		if (!entity)
			return null;

		return MRX_ShopComponent.Cast(entity.FindComponent(MRX_ShopComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static void FaceQuartermaster(int playerId)
	{
		MRX_ShopComponent shop = FindQuartermaster();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!shop || !character)
			return;

		IEntity quartermaster = shop.GetOwner();
		Print(string.Format("[CTR_DEV] quartermaster at %1 facing %2, parent %3, base at %4", quartermaster.GetOrigin(), quartermaster.GetYawPitchRoll()[0], quartermaster.GetParent(), COE_GameMode.GetInstance().GetMainBasePos()));
		vector front = quartermaster.GetOrigin() + quartermaster.GetTransformAxis(2) * 2.5;
		vector toQuartermaster = quartermaster.GetOrigin() - front;
		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, front, toQuartermaster.ToYaw());
		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		if (editable)
			editable.SetTransform(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenShop()
	{
		MRX_ShopComponent shop = FindQuartermaster();
		if (shop)
			MRX_ShopMenu.Open(shop);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenStash(notnull SCR_PlayerController controller)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return;

		IEntity stashPoint = CTR_DevTools.FindNear(gameMode.GetMainBasePos(), 60, MRX_StashPointComponent);
		if (stashPoint)
			controller.MRX_RequestStashOpen(stashPoint);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_GameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			CTR_DevFileCommands.Start();
	}
}
#endif
