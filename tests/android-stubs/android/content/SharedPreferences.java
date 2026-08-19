package android.content;
public class SharedPreferences {
    public Editor edit() { return new Editor(); }
    public static class Editor {
        public Editor putString(String k, String v) { return this; }
        public Editor remove(String k) { return this; }
        public void apply() {}
    }
}
