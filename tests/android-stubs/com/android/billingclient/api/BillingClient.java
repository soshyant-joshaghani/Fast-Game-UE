package com.android.billingclient.api;

import android.app.Activity;
import java.util.List;

public class BillingClient {
    public static class BillingResponseCode {
        public static final int OK = 0;
        public static final int USER_CANCELED = 1;
    }

    public static class ProductType {
        public static final String INAPP = "inapp";
    }

    public static Builder newBuilder(Activity activity) {
        return new Builder();
    }

    public static class Builder {
        public Builder setListener(PurchasesUpdatedListener listener) {
            return this;
        }

        public Builder enablePendingPurchases() {
            return this;
        }

        public BillingClient build() {
            return new BillingClient();
        }
    }

    public void startConnection(BillingClientStateListener listener) {}

    public void endConnection() {}

    public void queryPurchasesAsync(QueryPurchasesParams params, PurchasesResponseListener listener) {}

    public void queryProductDetailsAsync(
            QueryProductDetailsParams params, ProductDetailsResponseListener listener) {}

    public BillingResult launchBillingFlow(Activity activity, BillingFlowParams params) {
        return new BillingResult();
    }
}
