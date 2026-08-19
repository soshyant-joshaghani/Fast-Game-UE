package com.fastgame.store;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;

import java.util.Arrays;
import java.util.List;

import ir.myket.billingclient.IabHelper;
import ir.myket.billingclient.util.IabResult;
import ir.myket.billingclient.util.Inventory;
import ir.myket.billingclient.util.Purchase;

/**
 * Myket flavor — restore/query + optional purchase for Fast Game Submit Billing.
 *
 * Intent extras:
 *   openTheStorePage (boolean)
 *   storeProductId (String) — Fast Game Payment.StoreProductId
 *   storePublicKey (String, optional) — Myket RSA public key
 *
 * JNI (UE): OnStorePurchase(storeProductId, purchaseToken, alreadyOwned)
 * Unity: set FastGameStoreActivity.Listener
 */
public class FastGameStoreActivity extends Activity {

    public interface FastGameStoreListener {
        void onStorePurchase(String storeProductId, String purchaseToken, boolean alreadyOwned);
    }

    public static volatile FastGameStoreListener Listener;

    public native void OnStorePurchase(String storeProductId, String purchaseToken, boolean alreadyOwned);

    static final String MYKET_PACKAGE_NAME = "ir.mservices.market";
    static final String PREFS_NAME = "fast_game_iap";
    static final String PREFS_TOKEN_KEY = "purchase_token";
    static final String EXTRA_OPEN_STORE = "openTheStorePage";
    static final String EXTRA_STORE_PRODUCT_ID = "storeProductId";
    static final String EXTRA_STORE_PUBLIC_KEY = "storePublicKey";
    static final String DIALOG_TITLE = "Fast Game";

    IabHelper mHelper = null;
    Activity activity = this;

    boolean openTheStorePage = false;
    boolean notifyCpp = false;
    boolean purchaseFlag = false;
    boolean alreadyOwned = false;
    String purchaseToken = "";
    String storeProductId = "";

    IabHelper.QueryInventoryFinishedListener mGotInventoryListener = new IabHelper.QueryInventoryFinishedListener() {
        @Override
        public void onQueryInventoryFinished(IabResult result, Inventory inv) {
            if (mHelper == null) {
                finish();
                return;
            }
            if (result.isFailure()) {
                showErrorAndFinish(result.toString());
                return;
            }

            Purchase premiumPurchase = inv.getPurchase(storeProductId);
            if (premiumPurchase != null && storeProductId.equals(premiumPurchase.getSku())) {
                alreadyOwned = true;
                if (premiumPurchase.getToken() != null) {
                    purchaseToken = premiumPurchase.getToken();
                }
                if (openTheStorePage && purchaseToken != null && !purchaseToken.isEmpty()) {
                    purchaseFlag = true;
                    persistPurchaseToken(purchaseToken);
                }
            }

            if (!alreadyOwned && openTheStorePage) {
                mHelper.launchPurchaseFlow(activity, storeProductId, mPurchaseFinishedListener, "");
            } else {
                finish();
            }
        }
    };

    IabHelper.OnIabPurchaseFinishedListener mPurchaseFinishedListener = new IabHelper.OnIabPurchaseFinishedListener() {
        public void onIabPurchaseFinished(IabResult result, Purchase purchase) {
            if (mHelper == null) {
                finish();
                return;
            }
            if (result.isFailure()) {
                showErrorAndFinish(result.toString());
                return;
            }
            if (purchase != null && storeProductId.equals(purchase.getSku())) {
                purchaseFlag = true;
                alreadyOwned = false;
                if (purchase.getToken() != null) {
                    purchaseToken = purchase.getToken();
                }
                persistPurchaseToken(purchaseToken);
            }
            finish();
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        purchaseFlag = false;
        purchaseToken = "";
        alreadyOwned = false;
        notifyCpp = true;
        openTheStorePage = getIntent().getBooleanExtra(EXTRA_OPEN_STORE, false);
        String fromIntent = getIntent().getStringExtra(EXTRA_STORE_PRODUCT_ID);
        storeProductId = fromIntent != null ? fromIntent.trim() : "";
        String publicKey = getIntent().getStringExtra(EXTRA_STORE_PUBLIC_KEY);
        if (publicKey == null) {
            publicKey = "";
        }
        publicKey = publicKey.trim();
        clearPersistedPurchaseToken();

        if (storeProductId.isEmpty()) {
            notifyResult();
            showErrorAndFinish("storeProductId is empty");
            return;
        }

        if (!isMyketInstalled()) {
            notifyCpp = true;
            purchaseToken = "";
            purchaseFlag = false;
            AlertDialog dlg = new AlertDialog.Builder(activity)
                .setTitle(DIALOG_TITLE)
                .setMessage("Myket is not installed. Install Myket and try again.")
                .setPositiveButton("OK", (dialog, which) -> finish())
                .create();
            dlg.setCancelable(false);
            dlg.setCanceledOnTouchOutside(false);
            dlg.show();
            return;
        }

        if (publicKey.isEmpty()) {
            showErrorAndFinish("Myket storePublicKey extra is required");
            return;
        }

        mHelper = new IabHelper(this, publicKey);
        mHelper.startSetup(result -> {
            if (mHelper == null) {
                finish();
                return;
            }
            if (!result.isSuccess()) {
                showErrorAndFinish(result.toString());
                return;
            }
            List<String> skuList = Arrays.asList(storeProductId);
            mHelper.queryInventoryAsync(true, skuList, mGotInventoryListener);
        });
    }

    boolean isMyketInstalled() {
        try {
            getPackageManager().getPackageInfo(MYKET_PACKAGE_NAME, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    void showErrorAndFinish(String message) {
        AlertDialog dlg = new AlertDialog.Builder(activity)
            .setTitle(DIALOG_TITLE)
            .setMessage(message != null ? message : "Myket error")
            .setPositiveButton("OK", (dialog, which) -> finish())
            .create();
        dlg.setCanceledOnTouchOutside(false);
        dlg.show();
    }

    void persistPurchaseToken(String token) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().putString(PREFS_TOKEN_KEY, token != null ? token : "").apply();
    }

    void clearPersistedPurchaseToken() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().remove(PREFS_TOKEN_KEY).apply();
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
    public void onDestroy() {
        if (notifyCpp) {
            notifyResult();
            notifyCpp = false;
        }
        if (mHelper != null) {
            mHelper.dispose();
            mHelper = null;
        }
        super.onDestroy();
    }
}
