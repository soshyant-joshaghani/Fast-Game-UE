package android.app;
public class AlertDialog {
    public void setCancelable(boolean b) {}
    public void setCanceledOnTouchOutside(boolean b) {}
    public void show() {}
    public static class Builder {
        public Builder(Activity a) {}
        public Builder setTitle(String t) { return this; }
        public Builder setMessage(String m) { return this; }
        public Builder setPositiveButton(String t, android.content.DialogInterface.OnClickListener l) { return this; }
        public AlertDialog create() { return new AlertDialog(); }
    }
}
