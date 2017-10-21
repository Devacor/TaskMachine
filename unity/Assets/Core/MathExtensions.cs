using System;
using System.Collections.Generic;
using UnityEngine;

namespace SharedCore
{
    public static class MathExtensions
    {
        /// <summary>
        /// Weighted curve equivalent to classic simple easing methods:
        /// 1 == linear, 2 == quad, 3 == cubic, 4 == quart, 5 == quint
        /// Visual representations: http://easings.net/ (note, this implementation is custom and unrelated to that link, just equivalent math)
        /// </summary>
        public static float Mix(float start, float end, float percent, float strength = 1.0f)
        {
            return MixIn(start, end, percent, strength);
        }

        public static float MixIn(float start, float end, float percent, float strength = 1.0f)
        {
            return (float)Math.Pow(percent, strength) * (end - start) + start;
        }

        public static float MixOut(float start, float end, float percent, float strength = 1.0f)
        {
            return (1.0f - (float)Math.Pow(1.0f - percent, strength)) * (end - start) + start;
        }

        public static float MixInOut(float start, float end, float percent, float strength = 1.0f)
        {
            var halfRange = (end - start) / 2.0f + start;
            if (percent < .5f)
            {
                return MixIn(start, halfRange, percent * 2.0f, strength);
            }
            return MixOut(halfRange, end, (percent - .5f) * 2.0f, strength);
        }

        public static float MixOutIn(float start, float end, float percent, float strength = 1.0f)
        {
            var halfRange = (end - start) / 2.0f + start;
            if (percent < .5f)
            {
                return MixOut(start, halfRange, percent * 2.0f, strength);
            }
            return MixIn(halfRange, end, (percent - .5f) * 2.0f, strength);
        }

        /// <summary>
        /// UnMix will take a value and return a percent.
        /// IE: UnMix(10.0f, 10.0f, Mix(10.0f, 10.0f, .5f, 3.0f), 3.0f) == .5f
        /// In this way we can feed the output of Mix into UnMix to retrieve the percent from the value.
        /// </summary>
        public static float UnMix(float start, float end, float value, float strength = 1.0f)
        {
            return UnMixIn(start, end, value, strength);
        }

        public static float UnMixIn(float start, float end, float value, float strength = 1.0f)
        {
            return (float)Math.Pow((value - start) / (end - start), 1.0f / strength);
        }

        public static float UnMixOut(float start, float end, float value, float strength = 1.0f)
        {
            return (float)(Math.Pow((-1.0f * ((value - start) / (end - start) - 1.0f)), 1.0f / strength) - 1.0f) * -1.0f;
        }

        public static float UnMixInOut(float start, float end, float value, float strength = 1.0f)
        {
            var halfRange = (end - start) / 2.0f + start;
            if (value < halfRange)
            {
                return UnMixIn(start, halfRange, value, strength) / 2.0f;
            }
            return (UnMixOut(halfRange, end, value, strength) / 2.0f) + .5f;
        }

        public static float UnMixOutIn(float start, float end, float value, float strength = 1.0f)
        {
            var halfRange = (end - start) / 2.0f + start;
            if (value < halfRange)
            {
                return UnMixOut(start, halfRange, value, strength) / 2.0f;
            }
            return (UnMixIn(halfRange, end, value, strength) / 2.0f) + .5f;
        }
    }
}