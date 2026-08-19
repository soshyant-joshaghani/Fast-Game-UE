package com.fastgame.store;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.ArrayList;
import java.util.List;

/**
 * Google Play flavor — query + launchBillingFlow for Fast Game Submit Billing.
 *
 * Intent extras: openTheStorePage, storeProductId
 * JNI (UE): OnStorePurchase(storeProductId, purchaseToken, alreadyOwned)
 */
public class FastGameStoreActivity extends Activity implements PurchasesUpdatedListener {

    public interface FastGameStoreListener {
        void onStorePurchase(String storeProductId, String purchaseToken, boolean alreadyOwned);
    }

    public static volatile FastGameStoreListener Listener;

    public native void OnStorePurchase(String storeProductId, String purchaseToken, boolean alreadyOwned);

    static final String PLAY_PACKAGE = "com.android.vending";
    static final String PREFS_NAME = "fast_game_iap";
    static final String PREFS_TOKEN_KEY = "purchase_token";
    static final String EXTRA_OPEN_STORE = "openTheStorePage";
    static final String EXTRA_STORE_PRODUCT_ID = "storeProductId";
    static final String DIALOG_TITLE = "Fast Game";

    BillingClient billingClient;
    boolean openTheStorePage = false;
    boolean notifyCpp = false;
    boolean purchaseFlag = false;
    boolean alreadyOwned = false;
    String purchaseToken = "";
    String storeProductId = "";

    @Override
    public void onPurchasesUpdated(BillingResult billingResult, List<Purchase> purchases) {
        if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK && purchases != null) {
            for (Purchase p : purchases) {
                if (p.getProducts().contains(storeProductId) && p.getPurchaseToken() != null) {
                    purchaseToken = p.getPurchaseToken();
                    purchaseFlag = true;
                    alreadyOwned = false;
                    persistPurchaseToken(purchaseToken);
                    finish();
                    return;
                }
            }
        }
        if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.USER_CANCELED) {
            finish();
            return;
        }
        if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK) {
            showDialog("Google Play: " + billingResult.getDebugMessage());
        } else {
            finish();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        purchaseFlag = false;
        purchaseToken = "";
        alreadyOwned = false;
        notifyCpp = true;
        openTheStorePage = getIntent().getBooleanExtra(EXTRA_OPEN_STORE, false);
        String fromIntent = getIntent().getStringExtra(EXTRA_STORE_PRODUCT_ID);
        storeProductId = fromIntent != null ? fromIntent.trim() : "";
        clearPersistedPurchaseToken();

        if (storeProductId.isEmpty()) {
            showDialog("storeProductId is empty");
            return;
        }

        if (!isPlayStoreInstalled()) {
            showDialog("Google Play Store is not installed. Install it and try again.");
            return;
        }

        billingClient = BillingClient.newBuilder(this)
            .setListener(this)
            .enablePendingPurchases()
            .build();
        billingClient.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(BillingResult billingResult) {
                if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK) {
                    showDialog("Google Play setup failed: " + billingResult.getDebugMessage());
                    return;
                }
                queryExisting();
            }

            @Override
            public void onBillingServiceDisconnected() {
            }
        });
    }

    void queryExisting() {
        billingClient.queryPurchasesAsync(
            QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
            (result, purchases) -> {
                if (result.getResponseCode() == BillingClient.BillingResponseCode.OK && purchases != null) {
                    for (Purchase p : purchases) {
                        if (p.getProducts().contains(storeProductId) && p.getPurchaseToken() != null
                            && !p.getPurchaseToken().isEmpty()) {
                            alreadyOwned = true;
                            purchaseToken = p.getPurchaseToken();
                            if (openTheStorePage) {
                                purchaseFlag = true;
                                persistPurchaseToken(purchaseToken);
                            }
                            finish();
                            return;
                        }
                    }
                }
                if (!alreadyOwned && openTheStorePage) {
                    launchBuy();
                } else {
                    finish();
                }
            });
    }

    void launchBuy() {
        List<QueryProductDetailsParams.Product> products = new ArrayList<>();
        products.add(QueryProductDetailsParams.Product.newBuilder()
            .setProductId(storeProductId)
            .setProductType(BillingClient.ProductType.INAPP)
            .build());
        billingClient.queryProductDetailsAsync(
            QueryProductDetailsParams.newBuilder().setProductList(products).build(),
            (result, detailsList) -> {
                if (result.getResponseCode() != BillingClient.BillingResponseCode.OK
                    || detailsList == null || detailsList.isEmpty()) {
                    showDialog("Google Play product not found: " + storeProductId);
                    return;
                }
                ProductDetails details = detailsList.get(0);
                List<BillingFlowParams.ProductDetailsParams> params = new ArrayList<>();
                params.add(BillingFlowParams.ProductDetailsParams.newBuilder()
                    .setProductDetails(details)
                    .build());
                BillingFlowParams flow = BillingFlowParams.newBuilder()
                    .setProductDetailsParamsList(params)
                    .build();
                billingClient.launchBillingFlow(FastGameStoreActivity.this, flow);
            });
    }

    boolean isPlayStoreInstalled() {
        try {
            getPackageManager().getPackageInfo(PLAY_PACKAGE, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    void showDialog(String message) {
        AlertDialog dlg = new AlertDialog.Builder(this)
            .setTitle(DIALOG_TITLE)
            .setMessage(message != null ? message : "Google Play error")
            .setPositiveButton("OK", (d, w) -> finish())
            .create();
        dlg.setCanceledOnTouchOutside(false);
        dlg.show();
    }

    void persistPurchaseToken(String token) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .edit().putString(PREFS_TOKEN_KEY, token != null ? token : "").apply();
    }

    void clearPersistedPurchaseToken() {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(PREFS_TOKEN_KEY).apply();
    }

    void notifyResult() {
        String token = purchaseToken != null ? purchaseToken : "";
        if (purchaseFlag && !token.isEmpty()) {
            persistPurchaseToken(token);
        }
        FastGameStoreListener listener = Listener;
        if (listener != null) {
            listener.onStorePurchase(storeProductId, token, alreadyOwned);
        }
        try {
            OnStorePurchase(storeProductId, token, alreadyOwned);
        } catch (UnsatisfiedLinkError ignored) {
        }
    }

    @Override
    protected void onDestroy() {
        if (notifyCpp) {
            notifyResult();
            notifyCpp = false;
        }
        if (billingClient != null) {
            billingClient.endConnection();
            billingClient = null;
        }
        super.onDestroy();
    }
}
