package com.android.billingclient.api;

import java.util.List;

@FunctionalInterface
public interface ProductDetailsResponseListener {
    void onProductDetailsResponse(BillingResult billingResult, List<ProductDetails> productDetailsList);
}
