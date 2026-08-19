package ir.cafebazaar.poolakey.config

sealed class SecurityCheck {
    class Enable(val rsaPublicKey: String) : SecurityCheck()
    object Disable : SecurityCheck()
}

class PaymentConfiguration(val localSecurityCheck: SecurityCheck)
