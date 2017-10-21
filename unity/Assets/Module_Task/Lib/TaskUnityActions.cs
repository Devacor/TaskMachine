using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace SharedCore
{
    //Highly suggest not using this often. Only really glue code for legacy uses of web-like intricate coroutines so we can yield on them in the Task system.
    public class CoroutineTaskAdapter : ActionBase
    {
        private MonoBehaviour subject;
        private IEnumerator methodEnumerator;
        private Coroutine activeCoroutine;
        private bool complete = false;

        public CoroutineTaskAdapter(MonoBehaviour subject, IEnumerator methodEnumerator)
        {
            this.subject = subject;
            this.methodEnumerator = methodEnumerator;
        }

        public static Coroutine StartCoroutineThenCall(MonoBehaviour subject, IEnumerator method, Action onComplete)
        {
            return subject.StartCoroutine(StartCoroutineThenCallImplementation(method, onComplete));
        }

        private static IEnumerator StartCoroutineThenCallImplementation(IEnumerator method, Action onComplete)
        {
            yield return method;
            onComplete();
        }

        public override void OnStart(Task t)
        {
            if (subject)
                activeCoroutine = StartCoroutineThenCall(subject, methodEnumerator, () => { complete = true; });
        }

        public override void OnCancel(Task t)
        {
            if (activeCoroutine != null)
                subject.StopCoroutine(activeCoroutine);
            activeCoroutine = null;
        }

        public override void OnSuspend(Task t)
        {
            OnCancel(t);
        }

        public override void OnResume(Task t)
        {
            OnStart(t);
        }

        public override bool Update(Task t, double dt)
        {
            return complete || !subject || !subject.gameObject.activeInHierarchy || !subject.isActiveAndEnabled;
        }
    }

    public class RotateToFaceY : ActionBase
    {
        private Transform self;
        private Quaternion startingRotation;
        private float angle;
        private float duration;

        public RotateToFaceY(Transform self, float angleY, float duration)
        {
            this.self = self;
            this.angle = angleY;
            this.duration = duration;
        }

        public override void OnStart(Task t)
        {
            startingRotation = self.rotation;
            if (angle < startingRotation.eulerAngles.y)
                angle += 360;
        }

        public override bool Update(Task t, double dt)
        {
            self.rotation = Quaternion.Euler(startingRotation.eulerAngles.x, Mathf.Lerp(startingRotation.eulerAngles.y, angle, (float)t.elapsed() / duration), startingRotation.eulerAngles.z);
            return t.elapsed() >= duration;
        }
    }

}