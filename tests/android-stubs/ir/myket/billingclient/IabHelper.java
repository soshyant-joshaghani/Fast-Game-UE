package ir.myket.billingclient;

import android.app.Activity;
import ir.myket.billingclient.util.IabResult;
import ir.myket.billingclient.util.Inventory;
import ir.myket.billingclient.util.Purchase;

public class IabHelper {
    public interface OnIabSetupFinishedListener {
        void onIabSetupFinished(IabResult result);
    }
    public interface QueryInventoryFinishedListener {
        void onQueryInventoryFinished(IabResult result, Inventory inv);
    }
    public interface OnIabPurchaseFinishedListener {
        void onIabPurchaseFinished(IabResult result, Purchase purchase);
    }
    public IabHelper(Activity activity, String key) {}
    public void startSetup(OnIabSetupFinishedListener l) {}
    public void queryInventoryAsync(boolean q, java.util.List<String> skus, QueryInventoryFinishedListener l) {}
    public void launchPurchaseFlow(Activity a, String sku, OnIabPurchaseFinishedListener l, String payload) {}
    public void dispose() {}
}
