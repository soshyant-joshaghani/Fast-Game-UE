package ir.cafebazaar.poolakey

import android.content.Context
import ir.cafebazaar.poolakey.config.PaymentConfiguration
import ir.cafebazaar.poolakey.entity.PurchaseInfo
import ir.cafebazaar.poolakey.request.PurchaseRequest

class Connection {
    fun disconnect() {}
}

class Payment(context: Context, config: PaymentConfiguration) {
    fun connect(block: ConnectionCallback.() -> Unit): Connection {
        ConnectionCallback().block()
        return Connection()
    }

    fun getPurchasedProducts(block: PurchaseQueryCallback.() -> Unit) {
        PurchaseQueryCallback().block()
    }

    fun getSubscribedProducts(block: PurchaseQueryCallback.() -> Unit) {
        PurchaseQueryCallback().block()
    }

    fun purchaseProduct(
        registry: Any?,
        request: PurchaseRequest,
        block: PurchaseCallback.() -> Unit
    ) {
        PurchaseCallback().block()
    }
}

class ConnectionCallback {
    fun connectionSucceed(block: () -> Unit) {}
    fun connectionFailed(block: (Throwable) -> Unit) {}
    fun disconnected(block: () -> Unit) {}
}

class PurchaseQueryCallback {
    fun querySucceed(block: (List<PurchaseInfo>) -> Unit) {}
    fun queryFailed(block: (Throwable) -> Unit) {}
}

class PurchaseCallback {
    fun purchaseFlowBegan(block: () -> Unit) {}
    fun failedToBeginFlow(block: (Throwable) -> Unit) {}
    fun purchaseSucceed(block: (PurchaseInfo) -> Unit) {}
    fun purchaseCanceled(block: () -> Unit) {}
    fun purchaseFailed(block: (Throwable) -> Unit) {}
}
