package android.net;

public class ConnectivityManager {
    public Network getActiveNetwork() {
        return new Network();
    }

    public NetworkCapabilities getNetworkCapabilities(Network network) {
        return new NetworkCapabilities();
    }
}
