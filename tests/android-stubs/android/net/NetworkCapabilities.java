package android.net;

public class NetworkCapabilities {
    public static final int NET_CAPABILITY_INTERNET = 12;
    public static final int NET_CAPABILITY_VALIDATED = 16;

    public boolean hasCapability(int capability) {
        return true;
    }
}
