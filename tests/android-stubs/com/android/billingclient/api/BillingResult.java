package com.android.billingclient.api;

public class BillingResult {
    public int getResponseCode() {
        return BillingClient.BillingResponseCode.OK;
    }

    public String getDebugMessage() {
        return "";
    }
}
