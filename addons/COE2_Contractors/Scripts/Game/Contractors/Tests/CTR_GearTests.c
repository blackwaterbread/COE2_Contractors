#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
class CTR_GearTests
{
	static const ResourceName ION_RIFLEMAN = "{4B1455E84A41E460}Prefabs/Characters/Factions/INDFOR/RHS_ION/Character_RHS_ION_BaseLoadout.et";

	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_StarterKit());
		runner.Add(new CTR_Test_LastGear());
		runner.Add(new CTR_Test_SafeZone());
		runner.Add(new CTR_Test_StashPageProduct());
		runner.Add(new CTR_Test_Quartermaster());
	}
}

//------------------------------------------------------------------------------------------------
//! An ION rifleman (armor, helmet, AR-15) gets the starter kit: no helmet or armor, the M16A2 with magazines, all of it
//! issued and worth nothing to the shops.
class CTR_Test_StarterKit : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
		{
			Skip("no COE2 game mode");
			return;
		}

		vector position = gameMode.GetMainBasePos() + "0 0 40";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(CTR_GearTests.ION_RIFLEMAN), GetGame().GetWorld(), params);
		Check(character != null, "ION rifleman spawns");
		if (!character)
		{
			Finish();
			return;
		}

		array<ResourceName> kit = CTR_Settings.Get().GetStarterKit();
		int given = CTR_StarterKit.Apply(character, kit);
		CheckInt(given, kit.Count(), "every kit item given");

		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(character, items);
		int rifles, magazines, loaded, notIssued;
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (prefab == CTR_StarterKit.RIFLE)
				rifles++;
			else if (item.FindComponent(BaseMagazineComponent))
				magazines++;

			if (item.FindComponent(BaseMagazineComponent) && item.GetParent() && SCR_ResourceNameUtils.GetPrefabName(item.GetParent()) == CTR_StarterKit.RIFLE)
				loaded++;

			if (!MRX_IssuedItems.IsIssued(item))
				notIssued++;

		}

		CheckInt(rifles, 1, "one M16A2");
		CheckInt(magazines, 4, "four magazines");
		CheckInt(loaded, 1, "the rifle is loaded");
		CheckInt(notIssued, 0, "everything issued");

		SCR_CharacterInventoryStorageComponent clothing = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		Check(clothing && !clothing.GetClothFromArea(LoadoutHeadCoverArea), "no helmet");
		Check(clothing && !clothing.GetClothFromArea(LoadoutVestArea), "no vest");

		SCR_EntityHelper.DeleteEntityAndChildren(character);
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Last gear of a test owner: an ION rifleman's gear (one magazine half empty, one item issued) is saved, put on a
//! character wearing the starter kit with the same items and state, read back from storage by a new instance, and
//! gone after clearing.
class CTR_Test_LastGear : CTR_TestCase
{
	static const string OWNER = "ctr-test:last-gear";
	protected static const int HALF_AMMO = 7;

	protected ref CTR_LastGear m_Reader;
	protected IEntity m_Source;
	protected IEntity m_Target;
	protected int m_iSourceItems;
	protected int m_iWaitMs;
	protected bool m_bExpectGear;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !MRX_Marx.GetStash())
		{
			Skip("no COE2 game mode or stash service");
			return;
		}

		m_Source = SpawnRifleman(gameMode, 40);
		m_Target = SpawnRifleman(gameMode, 44);
		Check(m_Source && m_Target, "riflemen spawn");
		if (!m_Source || !m_Target)
		{
			End();
			return;
		}

		// The rifleman prefab carries no spare magazine: give it one.
		ChimeraCharacter source = ChimeraCharacter.Cast(m_Source);
		InventoryStorageManagerComponent manager = source.GetCharacterController().GetInventoryStorageManager();
		Check(manager.TrySpawnPrefabToStorage(CTR_StarterKit.MAGAZINE, null, -1, EStoragePurpose.PURPOSE_DEPOSIT), "a magazine fits the rifleman's gear");

		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(m_Source, items);
		m_iSourceItems = items.Count();
		BaseMagazineComponent halfMagazine;
		foreach (IEntity item : items)
		{
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine && magazine.GetMaxAmmoCount() > HALF_AMMO)
			{
				halfMagazine = magazine;
				break;
			}
		}

		Check(halfMagazine != null, "the rifleman carries a magazine");
		if (halfMagazine)
			halfMagazine.SetAmmoCount(HALF_AMMO);

		MRX_IssuedItems.Mark(items[0], false);
		CTR_StarterKit.Apply(m_Target, CTR_Settings.Get().GetStarterKit());

		CTR_LastGear lastGear = new CTR_LastGear();
		lastGear.Save(OWNER, m_Source);
		Check(lastGear.ApplyOwner(m_Target, OWNER), "the saved gear is put on");
		CheckGear(m_Target);

		// A new instance reads what was written (the stash service runs the owner's requests in order).
		m_bExpectGear = true;
		m_Reader = new CTR_LastGear();
		m_Reader.Load(OWNER);
		m_iWaitMs = 0;
		GetGame().GetCallqueue().CallLater(WaitForReader, 250, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGear(notnull IEntity character)
	{
		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(character, items);
		CheckInt(items.Count(), m_iSourceItems, "same number of items as the source");

		int issued, halfFull;
		foreach (IEntity item : items)
		{
			if (MRX_IssuedItems.IsIssued(item))
				issued++;

			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine && magazine.GetAmmoCount() == HALF_AMMO)
				halfFull++;
		}

		CheckInt(issued, 1, "the issued mark comes back, the starter kit's do not stay");
		Check(halfFull >= 1, "the half empty magazine keeps its ammo");
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForReader()
	{
		m_iWaitMs += 250;
		// Reading gear ends the wait; a cleared owner never gains any, so that case waits a fixed time for the read.
		bool done = m_bExpectGear && m_Reader.HasGear(OWNER);
		if (!done && m_iWaitMs < 8000 && (m_bExpectGear || m_iWaitMs < 3000))
			return;

		GetGame().GetCallqueue().Remove(WaitForReader);
		if (m_bExpectGear)
		{
			Check(done, "a new instance reads the saved gear from storage");
			m_Reader.Clear(OWNER);
			Check(!m_Reader.ApplyOwner(m_Target, OWNER), "nothing to put on after clearing");

			m_bExpectGear = false;
			m_Reader = new CTR_LastGear();
			m_Reader.Load(OWNER);
			m_iWaitMs = 0;
			GetGame().GetCallqueue().CallLater(WaitForReader, 250, true);
			return;
		}

		Check(!m_Reader.HasGear(OWNER), "storage holds no gear after clearing");
		End();
	}

	//------------------------------------------------------------------------------------------------
	protected void End()
	{
		if (m_Source)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Source);

		if (m_Target)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Target);

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnRifleman(notnull COE_GameMode gameMode, float offset)
	{
		vector position = gameMode.GetMainBasePos() + Vector(0, 0, offset);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(Resource.Load(CTR_GearTests.ION_RIFLEMAN), GetGame().GetWorld(), params);
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_SafeZone : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || gameMode.GetMainBasePos() == vector.Zero)
		{
			Skip("no COE2 main base");
			return;
		}

		vector basePosition = gameMode.GetMainBasePos();
		float radius = CTR_Settings.Get().m_fSafeZoneRadius;
		Check(CTR_SafeZone.IsInside(basePosition), "the base is in the safe zone");
		Check(CTR_SafeZone.IsInside(basePosition + Vector(radius - 1, 30, 0)), "inside the radius, any height");
		Check(!CTR_SafeZone.IsInside(basePosition + Vector(radius + 1, 0, 0)), "outside the radius");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The stash page product on a test owner: adds a page, reports the pages, refuses at the maximum.
class CTR_Test_StashPageProduct : CTR_TestCase
{
	static const string OWNER = "ctr-test:stash-pages";

	protected ref CTR_StashPageProduct m_Product;
	protected ref MRX_ShopItem m_Item;
	protected ref MRX_ShopProductCallback m_Callback;
	protected ref MRX_ShopProductStateCallback m_StateCallback;
	protected ref MRX_StashResultCallback m_ResetCallback;
	protected int m_iBasePages;
	protected bool m_bEnding;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 15000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_iBasePages = MRX_StashPages.GetBasePages();
		if (!MRX_Marx.GetStash() || m_iBasePages <= 0)
		{
			Skip("no stash service or stash without page limit");
			return;
		}

		m_Product = new CTR_StashPageProduct();
		m_Product.m_iMaxPages = m_iBasePages + 1;
		m_Item = MRX_ShopItem.Create("stash_page", ResourceName.Empty, 1000000);
		m_Item.m_Product = m_Product;
		Reset(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the test owner's extra pages, then starts or ends the test.
	protected void Reset(bool ending)
	{
		m_bEnding = ending;
		m_ResetCallback = new MRX_StashResultCallback();
		m_ResetCallback.GetOnResult().Insert(OnReset);
		MRX_TxContext context = MRX_TxContext.Create("test", "stash pages", "test:" + MRX_Marx.NewId());
		MRX_Marx.GetStash().SetProperty(OWNER, MRX_PropertyChange.Create(MRX_StashPages.PROPERTY, string.Empty), context, m_ResetCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnReset(MRX_StashResult result)
	{
		if (m_bEnding)
		{
			Finish();
			return;
		}

		m_Callback = new MRX_ShopProductCallback();
		m_Callback.GetOnResult().Insert(OnChecked);
		m_Product.Check(0, OWNER, m_Item, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChecked(MRX_EShopStatus status)
	{
		CheckInt(status, MRX_EShopStatus.OK, "check below the maximum");
		m_Callback = new MRX_ShopProductCallback();
		m_Callback.GetOnResult().Insert(OnDelivered);
		m_Product.Deliver(0, OWNER, m_Item, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDelivered(MRX_EShopStatus status)
	{
		CheckInt(status, MRX_EShopStatus.OK, "page delivered");
		m_StateCallback = new MRX_ShopProductStateCallback();
		m_StateCallback.GetOnResult().Insert(OnState);
		m_Product.GetState(0, OWNER, m_Item, m_StateCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnState(MRX_ShopProductState state)
	{
		Check(state && !state.m_bAvailable, "not available at the maximum");
		if (state)
			CheckString(state.m_sText, CTR_StashPageProduct.FormatState(m_iBasePages + 1, m_iBasePages + 1), "state text");

		m_Callback = new MRX_ShopProductCallback();
		m_Callback.GetOnResult().Insert(OnCheckedAtMaximum);
		m_Product.Check(0, OWNER, m_Item, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCheckedAtMaximum(MRX_EShopStatus status)
	{
		CheckInt(status, MRX_EShopStatus.LIMIT_REACHED, "check at the maximum");
		m_Callback = new MRX_ShopProductCallback();
		m_Callback.GetOnResult().Insert(OnDeliveredAtMaximum);
		m_Product.Deliver(0, OWNER, m_Item, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDeliveredAtMaximum(MRX_EShopStatus status)
	{
		CheckInt(status, MRX_EShopStatus.LIMIT_REACHED, "delivery at the maximum");
		Reset(true);
	}
}

//------------------------------------------------------------------------------------------------
//! The quartermaster stands at the base: a shop keeper with the services catalog (stash pages) and the trade action;
//! loadout prices come from the arsenal shops.
class CTR_Test_Quartermaster : CTR_TestCase
{
	protected static const float NEARBY_RADIUS = 40;
	protected ref array<IEntity> m_aNearby = {};

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.GetMainBase())
		{
			Skip("no COE2 main base");
			return;
		}

		GetGame().GetWorld().QueryEntitiesBySphere(gameMode.GetMainBase().GetOrigin(), NEARBY_RADIUS, CollectNearby);
		IEntity quartermaster;
		foreach (IEntity entity : m_aNearby)
		{
			MRX_ShopComponent shop = MRX_ShopComponent.Cast(entity.FindComponent(MRX_ShopComponent));
			if (shop && shop.GetShopId() == CTR_ShopPricing.SHOP_SERVICES)
				quartermaster = entity;
		}

		Check(quartermaster != null, "quartermaster at the base");
		if (quartermaster)
		{
			Check(ChimeraCharacter.Cast(quartermaster) != null, "the quartermaster is a character");
			Check(quartermaster.FindComponent(MRX_ShopKeeperComponent) != null, "shop keeper");
			DamageManagerComponent damage = DamageManagerComponent.Cast(quartermaster.FindComponent(DamageManagerComponent));
			Check(damage && !damage.IsDamageHandlingEnabled(), "takes no damage");

			MRX_ShopDefinition definition = MRX_ShopComponent.Cast(quartermaster.FindComponent(MRX_ShopComponent)).GetDefinition();
			MRX_ShopItem page;
			if (definition)
				page = definition.m_Catalog.FindItem("stash_page");

			Check(page && CTR_StashPageProduct.Cast(page.m_Product) != null, "sells stash pages");
			if (page)
				CheckInt(page.m_iPrice, 1000000, "stash page price");

			Check(definition && !definition.m_bAllowSell, "buys nothing back");
			Check(HasTradeAction(quartermaster), "trade action");
		}

		MRX_PriceList prices = MRX_Marx.GetPriceList();
		Check(prices != null, "loadout prices set");
		if (prices)
			Check(prices.GetBuyPrice(CTR_StarterKit.RIFLE) > 900, "M16A2 priced for loadouts");

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasTradeAction(notnull IEntity entity)
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (!actions)
			return false;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction action : list)
		{
			if (MRX_OpenShopAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNearby(IEntity entity)
	{
		m_aNearby.Insert(entity);
		return true;
	}
}
#endif
