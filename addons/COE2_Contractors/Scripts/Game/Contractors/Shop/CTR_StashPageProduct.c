//! Shop product: one more stash page (Marx_Stash extra pages), up to m_iMaxPages pages in all. Server.
[BaseContainerProps()]
class CTR_StashPageProduct : MRX_ShopProduct
{
	static const string LEDGER_SOURCE = "coe2_contractors";

	[Attribute("8", desc: "Most pages a stash can have, its own pages included", params: "1 50")]
	int m_iMaxPages;

	//------------------------------------------------------------------------------------------------
	override void Check(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		ListPages(ownerId, new CTR_StashPageCheck(callback, m_iMaxPages));
	}

	//------------------------------------------------------------------------------------------------
	override void Deliver(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		MRX_TxContext context = MRX_TxContext.Create(LEDGER_SOURCE, "stash_page", "stash_page:" + MRX_Marx.NewId());
		MRX_StashPages.AddPages(ownerId, 1, m_iMaxPages, context, new CTR_StashPageDelivery(callback));
	}

	//------------------------------------------------------------------------------------------------
	override void GetState(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductStateCallback callback)
	{
		ListPages(ownerId, new CTR_StashPageState(callback, m_iMaxPages));
	}

	//------------------------------------------------------------------------------------------------
	protected static void ListPages(string ownerId, notnull MRX_StashCallback callback)
	{
		MRX_StashService stash = MRX_Marx.GetStash();
		if (!stash)
		{
			callback.OnResult(MRX_EStashStatus.STORAGE_ERROR, null);
			return;
		}

		stash.List(ownerId, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Text of the shop window, e.g. "Stash: 3 / 8 pages".
	static string FormatState(int pages, int maxPages)
	{
		return string.Format("Stash: %1 / %2 pages", pages, maxPages);
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_StashPageCheck : MRX_StashCallback
{
	protected ref MRX_ShopProductCallback m_Callback;
	protected int m_iMaxPages;

	//------------------------------------------------------------------------------------------------
	void CTR_StashPageCheck(MRX_ShopProductCallback callback, int maxPages)
	{
		m_Callback = callback;
		m_iMaxPages = maxPages;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			m_Callback.OnResult(MRX_EShopStatus.REJECTED);
			return;
		}

		int pages = MRX_StashPages.GetPages(record);
		if (pages <= 0 || pages >= m_iMaxPages)
			m_Callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
		else
			m_Callback.OnResult(MRX_EShopStatus.OK);
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_StashPageState : MRX_StashCallback
{
	protected ref MRX_ShopProductStateCallback m_Callback;
	protected int m_iMaxPages;

	//------------------------------------------------------------------------------------------------
	void CTR_StashPageState(MRX_ShopProductStateCallback callback, int maxPages)
	{
		m_Callback = callback;
		m_iMaxPages = maxPages;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			m_Callback.OnResult(MRX_ShopProductState.Create(false, string.Empty));
			return;
		}

		int pages = MRX_StashPages.GetPages(record);
		m_Callback.OnResult(MRX_ShopProductState.Create(pages > 0 && pages < m_iMaxPages, CTR_StashPageProduct.FormatState(pages, m_iMaxPages)));
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_StashPageDelivery : MRX_StashPagesCallback
{
	protected ref MRX_ShopProductCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void CTR_StashPageDelivery(MRX_ShopProductCallback callback)
	{
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, bool limitReached, int pages)
	{
		if (limitReached)
			m_Callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
		else if (status == MRX_EStashStatus.OK)
			m_Callback.OnResult(MRX_EShopStatus.OK);
		else
			m_Callback.OnResult(MRX_EShopStatus.DELIVERY_FAILED);
	}
}
