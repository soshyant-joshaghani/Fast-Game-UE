package android.content.pm;
public class PackageManager {
    public PackageInfo getPackageInfo(String n, int f) throws NameNotFoundException {
        if (n == null || n.isEmpty()) throw new NameNotFoundException();
        return new PackageInfo();
    }
    public static class NameNotFoundException extends Exception {}
    public static class PackageInfo {}
}
