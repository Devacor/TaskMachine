using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace SharedCore
{
    public class BlockForSeconds : ActionBase
    {
        public override string DefaultName { get { return this.GetType().Name + "(" + seconds.ToString("n2") + ")"; } }

        private readonly double seconds = 0.0;

        public BlockForSeconds(double timeInSeconds)
        {
            seconds = timeInSeconds;
        }

        public override bool Update(Task a_self, double a_dt)
        {
            return a_self.localElapsed() >= seconds;
        }
    }

    public class BlockForFrames : ActionBase
    {
        public override string DefaultName { get { return this.GetType().Name + "(" + frames + ")"; } }

        private readonly int frames = 0;
        private int totalFrames = 0;

        public BlockForFrames(int timeInFrames = 1)
        {
            frames = timeInFrames;
        }

        public override bool Update(Task a_self, double a_dt)
        {
            return totalFrames++ >= frames;
        }
    }

    public class BlockWhile : ActionBase
    {
        private readonly Func<bool> predicate;

        public BlockWhile(Func<bool> predicate)
        {
            this.predicate = predicate;
        }

        public override bool Update(Task a_self, double a_dt)
        {
            return !predicate();
        }
    }

    public class BlockUntil : ActionBase
    {
        private readonly Func<bool> predicate;

        public BlockUntil(Func<bool> predicate)
        {
            this.predicate = predicate;
        }

        public override bool Update(Task a_self, double a_dt)
        {
            return predicate();
        }
    }

    public class LockWrapper : ActionBase
    {
        public const string LockEvent = "LockWrapper.Lock";
        public const string UnlockEvent = "UnlockWrapper.Lock";

        private static Dictionary<string, int> locks = new Dictionary<string, int>();

        public static bool Locked(string key)
        {
            int amount = 0;
            locks.TryGetValue(key, out amount);
            return amount > 0;
        }

        //Dangerous For Legacy Migration/Animations - Prefer to simply use a LockWrapper in a sequence.
        public static void Lock(string key)
        {
            int amount = 0;
            locks.TryGetValue(key, out amount);
            locks[key] = amount + 1;
            if (amount == 0)
            {
                Messenger.Broadcast(LockEvent, key);
            }
        }

        //Dangerous For Legacy Migration/Animations - Prefer to simply use a LockWrapper in a sequence.
        public static void Unlock(string key)
        {
            if (--locks[key] == 0)
            {
                Messenger.Broadcast(UnlockEvent, key);
            }
        }

        private List<string> keys = new List<string>();
        private bool locked = false;
        public LockWrapper(string key)
        {
            this.keys.Add(key);
        }
        public LockWrapper(List<string> keys)
        {
            this.keys = keys.ToList();
        }

        public override void OnStart(Task a_self)
        {
            Hold();
        }

        public override void OnCancel(Task a_self)
        {
            Release();
        }

        public override void OnFinishAll(Task a_self)
        {
            Release();
        }

        public override void OnSuspend(Task a_self)
        {
            Release();
        }

        public override void OnResume(Task a_self)
        {
            Hold();
        }

        public override void OnException(Task a_self, Exception e)
        {
            Debug.LogError("Exception Caught in LockWrapper: " + e);
            Release();
        }

        public override bool HandlesExceptions { get { return true; } }

        public void Hold()
        {
            if (!locked)
            {
                locked = true;
                foreach (var key in keys)
                    Lock(key);
            }
        }

        public void Release()
        {
            if (locked)
            {
                locked = false;
                foreach (var key in keys)
                    Unlock(key);
            }
        }
    }

    public class TweenFloat : ActionBase
    {
        private bool hasCaptureMethods = false;
        private Func<float> startValue;
        private Func<float> endValue;

        private float start;
        private float end;

        private float duration;
        private float strength;

        private Action<float> update;

        public TweenFloat(Func<float> startValue, Func<float> endValue, Action<float> update, float duration = 1.0f, float strength = 1.0f)
        {
            this.startValue = startValue;
            this.endValue = endValue;
            this.update = update;
            this.duration = duration;
            this.strength = strength;
            hasCaptureMethods = true;
        }

        public TweenFloat(float startValue, float endValue, Action<float> update, float duration = 1.0f, float strength = 1.0f)
        {
            this.start = startValue;
            this.end = endValue;
            this.update = update;
            this.duration = duration;
            this.strength = strength;
        }

        public override void OnStart(Task t)
        {
            if (hasCaptureMethods)
            {
                start = startValue();
                end = endValue();
            }
        }

        public override void OnResume(Task t)
        {
            if (hasCaptureMethods)
            {
                start = startValue();
                end = endValue();
            }
        }

        public override bool Update(Task t, double dt)
        {
            update(MathExtensions.Mix(start, end, Mathf.Clamp01((float)t.elapsed() / duration), strength));
            return t.elapsed() >= duration;
        }
    }
}