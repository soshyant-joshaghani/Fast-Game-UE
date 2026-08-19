package com.fastgame.store

import android.app.AlertDialog
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Bundle
import android.util.Log
import android.view.Gravity
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import ir.cafebazaar.poolakey.Connection
import ir.cafebazaar.poolakey.Payment
import ir.cafebazaar.poolakey.config.PaymentConfiguration
import ir.cafebazaar.poolakey.config.SecurityCheck
import ir.cafebazaar.poolakey.entity.PurchaseInfo
import ir.cafebazaar.poolakey.request.PurchaseRequest

/**
 * Cafe Bazaar flavor — restore/query + optional purchase for Fast Game Submit Billing.
 *
 * Intent extras: openTheStorePage, storeProductId, storePublicKey (RSA from Editor, not JWT).
 * JNI (UE): OnStorePurchase(storeProductId, purchaseToken, alreadyOwned)
 * Unity: FastGameStoreActivity.Listener
 *
 * Poolakey unbinds while the Cafe Bazaar UI is on top. Do not finish() on disconnected —
 * that closes the billing screen in a blink with no dialog.
 */
class FastGameStoreActivity : ComponentActivity() {

    fun interface FastGameStoreListener {
        fun onStorePurchase(storeProductId: String, purchaseToken: String, alreadyOwned: Boolean)
    }

    external fun OnStorePurchase(storeProductId: String, purchaseToken: String, alreadyOwned: Boolean)

    var notifyCpp: Boolean = false
    var openTheStorePage: Boolean = false
    var purchaseFlag: Boolean = false
    var alreadyOwned: Boolean = false
    var purchaseToken: String = ""
    var storeProductId: String = ""
    var purchaseFlowActive: Boolean = false

    var paymentConnection: Connection? = null
    val activity = this
    var rsaPublicKeyNormalized: String = ""
    var triedWithoutLocalRsa: Boolean = false
    var afterPurchaseUi: Boolean = false

    companion object {
        @JvmField
        @Volatile
        var Listener: FastGameStoreListener? = null

        const val MARKET_PACKAGE = "com.farsitel.bazaar"
        const val PREFS_NAME = "fast_game_iap"
        const val PREFS_TOKEN_KEY = "purchase_token"
        const val EXTRA_OPEN_STORE = "openTheStorePage"
        const val EXTRA_STORE_PRODUCT_ID = "storeProductId"
        const val EXTRA_STORE_PUBLIC_KEY = "storePublicKey"
        const val DIALOG_TITLE = "Fast Game"
        const val TAG = "FastGameStore"

        fun normalizeRsaPublicKey(raw: String): String {
            var t = raw.trim()
            if (t.isEmpty()) return ""
            t = t.replace("-----BEGIN PUBLIC KEY-----", "")
                .replace("-----END PUBLIC KEY-----", "")
                .replace("-----BEGIN RSA PUBLIC KEY-----", "")
                .replace("-----END RSA PUBLIC KEY-----", "")
            return t.replace("\\s".toRegex(), "")
        }
    }

    fun isMarketAppInstalled(): Boolean {
        return try {
            packageManager.getPackageInfo(MARKET_PACKAGE, 0)
            true
        } catch (_: PackageManager.NameNotFoundException) {
            false
        }
    }

    private fun isNetworkConnected(): Boolean {
        val cm = getSystemService(CONNECTIVITY_SERVICE) as? ConnectivityManager ?: return false
        val network = cm.activeNetwork ?: return false
        val capabilities = cm.getNetworkCapabilities(network) ?: return false
        return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
            capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
    }

    private fun showLoading() {
        val tv = TextView(this)
        tv.text = "Loading ..."
        tv.setTextColor(Color.WHITE)
        tv.textSize = 30f
        tv.gravity = Gravity.CENTER
        tv.setBackgroundColor(Color.BLACK)
        setContentView(tv)
    }

    private fun showDialog(message: String, thenFinish: Boolean = true) {
        Log.e(TAG, message)
        if (isFinishing) return
        runOnUiThread {
            if (isFinishing) return@runOnUiThread
            try {
                val dlg = AlertDialog.Builder(activity)
                    .setTitle(DIALOG_TITLE)
                    .setMessage(message)
                    .setPositiveButton("OK") { _, _ ->
                        if (thenFinish) finish()
                    }
                    .create()
                dlg.setCanceledOnTouchOutside(false)
                dlg.setCancelable(false)
                dlg.show()
            } catch (e: Exception) {
                Log.e(TAG, "showDialog failed", e)
                if (thenFinish) finish()
            }
        }
    }

    private fun persistPurchaseToken(token: String) {
        getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(PREFS_TOKEN_KEY, token)
            .apply()
    }

    private fun clearPersistedPurchaseToken() {
        getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .remove(PREFS_TOKEN_KEY)
            .apply()
    }

    /** Poolakey 2.x PurchaseInfo.purchaseToken — required for Fast Game Submit Billing. */
    fun extractPurchaseToken(purchaseEntity: Any): String {
        if (purchaseEntity is PurchaseInfo) {
            val token = purchaseEntity.purchaseToken
            if (token.isNotBlank()) return token
        }
        return try {
            val cls = purchaseEntity.javaClass
            for (name in arrayOf("purchaseToken", "token", "purchaseTokenValue")) {
                try {
                    val field = cls.getDeclaredField(name)
                    field.isAccessible = true
                    val value = field.get(purchaseEntity) as? String
                    if (!value.isNullOrBlank()) return value
                } catch (_: NoSuchFieldException) {
                }
                try {
                    val getter = "get" + name.replaceFirstChar { it.uppercase() }
                    val method = cls.getMethod(getter)
                    val value = method.invoke(purchaseEntity) as? String
                    if (!value.isNullOrBlank()) return value
                } catch (_: Exception) {
                }
            }
            Log.e(TAG, "No purchaseToken on ${cls.name}")
            ""
        } catch (e: Exception) {
            Log.e(TAG, "extractPurchaseToken failed", e)
            ""
        }
    }

    private fun notifyResult() {
        val token = purchaseToken
        if (purchaseFlag && token.isNotEmpty()) {
            persistPurchaseToken(token)
        }
        Listener?.onStorePurchase(storeProductId, token, alreadyOwned)
        try {
            OnStorePurchase(storeProductId, token, alreadyOwned)
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    private fun skuId(product: PurchaseInfo): String = product.productId.trim()

    private fun beginPoolakey(strictRsa: Boolean) {
        val security: SecurityCheck = if (strictRsa && rsaPublicKeyNormalized.isNotEmpty()) {
            SecurityCheck.Enable(rsaPublicKey = rsaPublicKeyNormalized)
        } else {
            Log.w(TAG, "Poolakey inventory without local RSA verify")
            SecurityCheck.Disable
        }
        val payment = Payment(context = this, config = PaymentConfiguration(localSecurityCheck = security))
        try {
            paymentConnection?.disconnect()
        } catch (_: Exception) {
        }
        paymentConnection = payment.connect {
            connectionSucceed {
                Log.i(TAG, "Poolakey connected sku='$storeProductId' rsaVerify=$strictRsa")
                queryInventory(payment)
            }
            connectionFailed { throwable ->
                Log.e(TAG, "Poolakey connectionFailed", throwable)
                if (!isMarketAppInstalled()) {
                    showDialog("Cafe Bazaar is not installed. Install Cafe Bazaar and try again.")
                } else if (!isNetworkConnected()) {
                    showDialog("No internet connection. Check your network and try again.")
                } else {
                    showDialog(throwable.message ?: "Cafe Bazaar connection failed")
                }
            }
            disconnected {
                // Bazaar unbinds while its purchase UI is in front. Finishing here kills the flow
                // in a blink (old IranStoresBridge also finished here — keep the Activity alive).
                Log.w(TAG, "Poolakey disconnected sku='$storeProductId' flow=$purchaseFlowActive")
            }
        }
    }

    private fun queryInventory(payment: Payment) {
        payment.getPurchasedProducts {
            querySucceed { inapps ->
                try {
                    payment.getSubscribedProducts {
                        querySucceed { subs ->
                            onInventory(payment, inapps + subs)
                        }
                        queryFailed {
                            Log.w(TAG, "getSubscribedProducts failed; using in-app only")
                            onInventory(payment, inapps)
                        }
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "getSubscribedProducts missing", e)
                    onInventory(payment, inapps)
                }
            }
            queryFailed { throwable ->
                if (!isNetworkConnected()) {
                    showDialog("No internet connection. Check your network and try again.")
                } else {
                    showDialog(
                        (throwable.message ?: "") +
                            "\nCafe Bazaar inventory failed. Confirm Cafe Bazaar is installed and you are signed in."
                    )
                }
            }
        }
    }

    private fun onInventory(payment: Payment, products: List<PurchaseInfo>) {
        val ids = products.joinToString(",") { skuId(it) }
        Log.i(TAG, "inventory count=${products.size} ids=[$ids] want='$storeProductId'")
        val owned = products.firstOrNull { skuId(it).equals(storeProductId, ignoreCase = true) }
        if (owned == null && !triedWithoutLocalRsa) {
            triedWithoutLocalRsa = true
            Log.w(
                TAG,
                "SKU '$storeProductId' missing after RSA-verified query; retry without local RSA (Poolakey drops receipts whose signature does not match Editor RSA)"
            )
            beginPoolakey(strictRsa = false)
            return
        }
        if (owned != null) {
            alreadyOwned = true
            val existingToken = extractPurchaseToken(owned)
            if (existingToken.isNotEmpty()) {
                purchaseToken = existingToken
            }
            Log.i(TAG, "already owned sku='$storeProductId' tokenChars=${purchaseToken.length}")
            if (openTheStorePage && purchaseToken.isEmpty()) {
                Log.e(TAG, "already owned but purchaseToken empty — not starting a new purchase")
            }
            if (alreadyOwned && openTheStorePage && purchaseToken.isNotEmpty()) {
                purchaseFlag = true
                persistPurchaseToken(purchaseToken)
            }
            if (!openTheStorePage) {
                @Suppress("DEPRECATION")
                overridePendingTransition(0, 0)
            }
            finish()
            return
        }
        if (openTheStorePage && !afterPurchaseUi) {
            startPurchase(payment)
        } else {
            if (!openTheStorePage || afterPurchaseUi) {
                @Suppress("DEPRECATION")
                overridePendingTransition(0, 0)
            }
            finish()
        }
    }

    private fun startPurchase(payment: Payment) {
        val purchaseRequest = PurchaseRequest(
            productId = storeProductId,
            payload = ""
        )
        purchaseFlowActive = true
        Log.i(TAG, "purchaseProduct begin sku='$storeProductId'")
        payment.purchaseProduct(
            registry = activityResultRegistry,
            request = purchaseRequest
        ) {
            purchaseFlowBegan {
                Log.i(TAG, "purchaseFlowBegan sku='$storeProductId'")
            }
            failedToBeginFlow { throwable ->
                purchaseFlowActive = false
                val detail = throwable.message ?: "unknown"
                showDialog(
                    "Cafe Bazaar could not start SKU '$storeProductId'.\n$detail\n" +
                        "store_skus.caffebazar must be the Cafe Bazaar console id (old APK used LittleGuardiansGame), not full_map / full_game."
                )
            }
            purchaseSucceed { purchaseEntity ->
                purchaseFlowActive = false
                purchaseToken = extractPurchaseToken(purchaseEntity)
                if (purchaseToken.isEmpty()) {
                    Log.e(TAG, "Bazaar purchaseSucceed but purchaseToken empty — re-query inventory")
                    requeryAfterPurchaseUi(payment)
                    return@purchaseSucceed
                }
                purchaseFlag = true
                alreadyOwned = false
                persistPurchaseToken(purchaseToken)
                finish()
            }
            purchaseCanceled {
                purchaseFlowActive = false
                Log.i(TAG, "purchaseCanceled sku='$storeProductId' — re-query (already-bought UI often cancels)")
                requeryAfterPurchaseUi(payment)
            }
            purchaseFailed { throwable ->
                purchaseFlowActive = false
                Log.w(TAG, "purchaseFailed — re-query inventory", throwable)
                requeryAfterPurchaseUi(payment)
            }
        }
    }

    /** Cafe Bazaar "already bought" often returns cancel; inventory then has the token. */
    private fun requeryAfterPurchaseUi(payment: Payment) {
        afterPurchaseUi = true
        triedWithoutLocalRsa = true
        Log.i(TAG, "re-query inventory after Bazaar purchase UI sku='$storeProductId'")
        queryInventory(payment)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        purchaseFlag = false
        purchaseToken = ""
        alreadyOwned = false
        purchaseFlowActive = false
        notifyCpp = true
        openTheStorePage = intent.getBooleanExtra(EXTRA_OPEN_STORE, false)
        if (openTheStorePage) {
            showLoading()
        } else {
            window.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(Color.TRANSPARENT))
            @Suppress("DEPRECATION")
            overridePendingTransition(0, 0)
        }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                // Keep the loading activity until Poolakey finishes, matching the old Bazaar bridge.
            }
        })

        val fromIntent = intent.getStringExtra(EXTRA_STORE_PRODUCT_ID)
        storeProductId = if (!fromIntent.isNullOrBlank()) fromIntent.trim() else ""
        rsaPublicKeyNormalized = normalizeRsaPublicKey(
            intent.getStringExtra(EXTRA_STORE_PUBLIC_KEY).orEmpty()
        )
        triedWithoutLocalRsa = false
        clearPersistedPurchaseToken()
        Log.i(
            TAG,
            "onCreate sku='$storeProductId' openStore=$openTheStorePage rsaChars=${rsaPublicKeyNormalized.length} rsaPrefix=${rsaPublicKeyNormalized.take(12)}"
        )

        if (storeProductId.isEmpty()) {
            showDialog("storeProductId is empty — set store_skus.caffebazar in Fast Game Editor to the Cafe Bazaar SKU (not the Fast Game map id).")
            return
        }

        if (!isMarketAppInstalled()) {
            purchaseToken = ""
            purchaseFlag = false
            showDialog("Cafe Bazaar is not installed. Install Cafe Bazaar and try again.")
            return
        }

        if (rsaPublicKeyNormalized.isEmpty()) {
            showDialog("Cafe Bazaar RSA public key is missing. Save it in Fast Game Editor payment config (not the JWT / api_secret).")
            return
        }

        beginPoolakey(strictRsa = true)
    }

    override fun onDestroy() {
        if (notifyCpp) {
            notifyResult()
            notifyCpp = false
        }
        try {
            paymentConnection?.disconnect()
        } catch (_: Exception) {
        }
        super.onDestroy()
    }
}
