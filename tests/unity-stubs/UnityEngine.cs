using System;
using System.Collections.Generic;

namespace UnityEngine
{
    public class Object
    {
        public static void Destroy(Object obj) {}
        public static void DontDestroyOnLoad(Object obj) {}
        public static T FindObjectOfType<T>() where T : Object => null;
    }

    public class Component : Object
    {
        public GameObject gameObject { get; } = new GameObject();
        public Transform transform { get; } = new Transform();
        public T GetComponent<T>() => default;
        public T GetComponentInParent<T>() => default;
        public T GetComponentInChildren<T>() => default;
    }

    public class Transform : Component
    {
        public Transform parent { get; }
    }

    public class Behaviour : Component {}

    public class MonoBehaviour : Behaviour {}

    public class GameObject : Object
    {
        public void SetActive(bool active) {}
        public T GetComponent<T>() => default;
        public T[] GetComponents<T>() => Array.Empty<T>();
        public T[] GetComponentsInChildren<T>(bool includeInactive) => Array.Empty<T>();
    }

    public static class Debug
    {
        public static void Log(object message) {}
        public static void LogWarning(object message) {}
        public static void LogError(object message) {}
    }

    public static class Application
    {
        public static void OpenURL(string url) {}
    }

    public static class PlayerPrefs
    {
        static readonly Dictionary<string, string> Store = new Dictionary<string, string>();

        public static string GetString(string key, string defaultValue = "") =>
            Store.TryGetValue(key ?? "", out var v) ? v : (defaultValue ?? "");

        public static void SetString(string key, string value)
        {
            if (string.IsNullOrEmpty(key)) return;
            Store[key] = value ?? "";
        }

        public static void DeleteKey(string key)
        {
            if (!string.IsNullOrEmpty(key)) Store.Remove(key);
        }

        public static void Save() {}
    }

    public static class JsonUtility
    {
        public static string ToJson(object obj) => "{}";
        public static T FromJson<T>(string json) => default;
    }

    public class HeaderAttribute : Attribute
    {
        public HeaderAttribute(string header) {}
    }

    public class TooltipAttribute : Attribute
    {
        public TooltipAttribute(string tooltip) {}
    }

    public class AddComponentMenuAttribute : Attribute
    {
        public AddComponentMenuAttribute(string menuName) {}
    }

    public class DisallowMultipleComponentAttribute : Attribute {}

    public class AndroidJavaObject : IDisposable
    {
        public AndroidJavaObject(string className, params object[] args) {}
        public T Call<T>(string method, params object[] args) => default;
        public void Call(string method, params object[] args) {}
        public T GetStatic<T>(string field) => default;
        public void SetStatic(string field, object value) {}
        public void Dispose() {}
    }

    public class AndroidJavaClass : AndroidJavaObject
    {
        public AndroidJavaClass(string className) : base(className) {}
    }

    public class AndroidJavaProxy
    {
        public AndroidJavaProxy(string javaInterface) {}
    }
}

namespace UnityEngine.Events
{
    public delegate void UnityAction();

    public class UnityEvent
    {
        public void AddListener(UnityAction call) {}
        public void Invoke() {}
    }

    public class UnityEvent<T0> : UnityEvent
    {
        public void AddListener(UnityAction<T0> call) {}
        public void Invoke(T0 arg0) {}
    }

    public class UnityEvent<T0, T1> : UnityEvent
    {
        public void AddListener(UnityAction<T0, T1> call) {}
        public void Invoke(T0 arg0, T1 arg1) {}
    }

    public class UnityEvent<T0, T1, T2> : UnityEvent
    {
        public void AddListener(UnityAction<T0, T1, T2> call) {}
        public void Invoke(T0 arg0, T1 arg1, T2 arg2) {}
    }

    public delegate void UnityAction<T0>(T0 arg0);
    public delegate void UnityAction<T0, T1>(T0 arg0, T1 arg1);
    public delegate void UnityAction<T0, T1, T2>(T0 arg0, T1 arg1, T2 arg2);
}

namespace UnityEngine.Serialization
{
    public class FormerlySerializedAsAttribute : Attribute
    {
        public FormerlySerializedAsAttribute(string name) {}
    }
}

namespace UnityEngine.Networking
{
    public class DownloadHandler
    {
        public string text => "{}";
    }

    public class DownloadHandlerBuffer : DownloadHandler {}

    public class UploadHandler {}

    public class UploadHandlerRaw : UploadHandler
    {
        public UploadHandlerRaw(byte[] data) {}
    }

    public class UnityWebRequestAsyncOperation
    {
        public bool isDone => true;
    }

    public class UnityWebRequest : IDisposable
    {
        public enum Result
        {
            InProgress,
            Success,
            ConnectionError,
            ProtocolError,
            DataProcessingError,
        }

        public UnityWebRequest(string url, string method) {}
        public UploadHandler uploadHandler { get; set; }
        public DownloadHandler downloadHandler { get; set; } = new DownloadHandlerBuffer();
        public bool isNetworkError => false;
        public bool isHttpError => false;
        public Result result => Result.Success;
        public long responseCode => 200;
        public string error => null;
        public void SetRequestHeader(string name, string value) {}
        public UnityWebRequestAsyncOperation SendWebRequest() => new UnityWebRequestAsyncOperation();
        public void Dispose() {}
        public static string EscapeURL(string s) => Uri.EscapeDataString(s ?? "");
    }
}
