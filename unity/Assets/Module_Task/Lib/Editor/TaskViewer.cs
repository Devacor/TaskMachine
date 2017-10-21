#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System;

namespace SharedCore
{
    [CustomEditor(typeof(RootTask))]
    public class TaskViewer : UnityEditor.Editor
    {
        private Dictionary<Task, bool> visible = new Dictionary<Task, bool>();
        private List<Task> active = new List<Task>();

        private List<Color> colors = new List<Color> { new Color(213 / 255f, 137 / 255f, 4 / 255f), new Color(79 / 255f, 115 / 255f, 184 / 255f), new Color(186 / 255f, 71 / 255f, 128 / 255f), new Color(55 / 255f, 116 / 255f, 176 / 255f), new Color(124 / 255f, 158 / 255f, 58 / 255f) };

        private bool showAllSequential = true;

        public override void OnInspectorGUI()
        {
            Task ourTask = ((RootTask)target).task;

            active.Clear();
            RenderTask(ourTask);

            foreach (var item in visible.ToList())
            {
                if (!active.Contains(item.Key))
                {
                    visible.Remove(item.Key);
                }
            }

            EditorUtility.SetDirty(target);
        }

        private void RenderTask(Task task, int indentLevel = 0)
        {
            if (!visible.ContainsKey(task))
            {
                visible.Add(task, true);
            }
            active.Add(task);
            string indentString = new string(' ', indentLevel * 2);

            var style = GetFoldoutStyle(indentLevel);

            int subtasks = task.parallelTasks.Count;
            subtasks += showAllSequential ? task.sequentialTasks.Count : ((task.localTaskComplete() || task.alwaysRunChildren()) && task.sequentialTasks.Count > 0) ? 1 : 0;
            string activeIndicator = !task.childTasksBlockedByLocal() ? "|" : (task.localTaskStarted() || task.localTaskComplete() ? "" : "~");
            var showFoldout = EditorGUILayout.Foldout(visible[task], indentString + activeIndicator + task.name() + " [" + subtasks.ToString() + "]: " + Math.Round(task.elapsed(), 2).ToString(), style);
            visible[task] = showFoldout;
            if (showFoldout)
            {
                if (!task.isCancelled())
                {
                    foreach (var subTask in task.parallelTasks)
                    {
                        RenderTask(subTask, indentLevel + 1);
                    }

                    if (showAllSequential)
                    {
                        foreach (var subTask in task.sequentialTasks)
                        {
                            RenderTask(subTask, indentLevel + 1);
                        }
                    }
                    else
                    {
                        if (task.localTaskComplete() || task.alwaysRunChildren())
                        {
                            if (task.sequentialTasks.Count > 0)
                            {
                                RenderTask(task.sequentialTasks[0], indentLevel + 1);
                            }
                        }
                    }
                }
            }
        }


        private GUIStyle GetFoldoutStyle(int indentLevel)
        {
            var style = new GUIStyle(EditorStyles.foldout);
            style.fontStyle = FontStyle.Bold;
            int colorIndex = indentLevel % colors.Count;
            style.normal.textColor = colors[colorIndex];
            style.onNormal.textColor = colors[colorIndex];
            style.hover.textColor = colors[colorIndex];
            style.onHover.textColor = colors[colorIndex];
            style.focused.textColor = colors[colorIndex];
            style.onFocused.textColor = colors[colorIndex];
            style.active.textColor = colors[colorIndex];
            style.onActive.textColor = colors[colorIndex];
            return style;
        }
    }
}
#endif