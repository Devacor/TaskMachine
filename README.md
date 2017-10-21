Task Machine is a sequencing library I originally wrote in C++ to address animation needs over multiple frames. This is not a threading system, but rather, a coroutine style sequencing system. A task is meant to run on a single thread over multiple frames. While I originally wrote this in C++ I found it useful enough to port to C#, despite Unity's fantastic coroutine support it can be handy to break out of the unity timestep and take a little more fine grained control over update methods.

This project is structured so you can easily view the C++ project (the Task system itself compiles with clang and visual studio 2017, but the mainExample.cpp file which is only meant as a demonstration is windows specific due to keyboard input.)

Predictably the C++ version is in "cpp" and the C#/Unity version is in "unity". Both systems have the same capabilities but their dependencies are laid out a little differently. Looking at task.hpp or task.cs gives a good view of the core of the system.

This system has been ported by others. I am aware of an inspired library used in Duck Game [http://store.steampowered.com/app/312530/Duck_Game/] and a light-weight javascript port here [https://github.com/iwilliams/task-action] named task-action to avoid a clash with an existing (but unsimilar/unmaintained) javascript library called taskmachine.

The C++/C# versions are called Task Machine. The following was written to describe this system from a Unity standpoint. From a C++ standpoint, there is no current standard solution to sanely manage multi-frame behaviours like this. Upcoming coroutine papers look promising, however! The full version fo my task system has integration with chaiscript and cereal enabling access to the sequencing system in script and serialization/deserialization of tasks for potential network synchronization. This has been stripped out of this version to make a clean build with no external dependencies.

#Notes Specific to C# Follow, The C++ Interface is Very Similar

# Task System

## Task

Task exists as a modular nestable sequencing data structure. In many ways coroutines seek to address the same issue of spreading work over multiple frames.

In their simplest case at a single level of complexity coroutines are very useful and straight forward. It is when you dig just a little deeper that the issues come up:

1. Exception handling for multiple levels is especially messy, if a coroutine throws, it cancells the parent coroutines all the way up the stack. Task comes with a built in .onException callback which you can use to handle (or log and re-throw) exceptions. You still have to think about exceptions, but at least you have the tools to handle them on a per task basis, or ignore them at a lower level and catch at a higher level without trashing your entire logical tree.
2. Trying to cancel a currently running coroutine with more than one level of yield StartCoroutine is also unweildly. You need to manually maintain references to each and every coroutine in the stack you want to cancel, and, starting at the deepest level, call StopCoroutine all the way up the chain.
3. There is no easy to visualize method for viewing the state of coroutines. This is really just a "nice to have", but the callstack is not terribly useful in the case of a coroutine, there isn't much context.
4. Coroutines are intimately tied to the unity timestep. This means that if you crank your time-scale way up, to 100 or so, unless you are *very* careful at every point where you handle time things can become very unstable, and there will be a lot of manual loops to handle large delta times.  
Task has a built in system for enforcing a fixed time step. You can choose to clamp only the local update function for a task with a fixed time step by calling task.localInterval(.1), or affect all child tasks by calling task.interval(.1).  This means that if Time.smoothDeltaTime is somehow 1.0 seconds and you have an interval of .1 you can reliably expect your update methods to be called 10 times with a delta of .1 each time. This works on total time as well, so if one frame you have a smoothDeltaTime of .25, it will run 2 times with .1 each time.  If the next frame you get another .25 it will be a total of .5 seconds elapsed and will be called 3 times with .1 to make up.
5. Coroutines rely on monobehaviors. Sometimes you want to run a coroutine on something that is not attached to a gameObject and this is impossible.


This all comes at a cost in terms of readability, but it is not **much** of a cost once you get used to the idiomatic method of writing tasks. The example I have written of a comparable task vs coroutine ends up clocking in with **49** lines of code for the coroutine and **50** lines of code for the tasks.

## Pro/Con of Tasks:

### Task Pros: 
* Visible Hierarchy: Easy to see at a glance what state your application is in.
* OnException: Able to catch exceptions at every level of the task system (or let it fall through to the next level up).
* No Monobehaviour dependency: There are several cases where the class you want to execute a coroutine from does not derive from a monobehavior. You could run it on a gameObject, but sometimes this is not natural either. Tasks allow for greater separation of concerns for non game-object sequenced events.
* Better concept of context flow. The following delegates: onStart, onFinish, onFinishAll, onSuspend, onResume, and onCancel all provide explicit areas to hook in code. onFinishAll or onCancel or onException are the only 3 exit paths from a task, so resource clean-up logic can be easier than with coroutines which might throw an exception and abort at any point in time without notice.
* Able to cancel a Task and all children easier than a coroutine. Good luck stopping a nested coroutine without it being a hairy mess.
* Able to force a stable timestep (or multiple stable timesteps) more easily. More flexible than FixedUpdate.

### Task Cons:
* Syntax is not as natural as the IEnumerator + yield combination which makes resumable functions. Tasks are like Update functions in the way they are written which are strung together in a well defined sequence.
* Need to consider the lifespan of the task vs the lifespan of the gameObjects it might be touching. Sometimes coroutines which act on their own game object explicitly can be more graceful to clean up. This is typically a problem for both systems though.
* Not as thoroughly tested. We have fairly extensive use of Tasks, and they work very well. I am not certain it is a bullet-proof library yet and would like to write some unit tests for it when time permits.
* Not a widely recognized and supported concept when compared to coroutines, new programmers will need to read a bit to learn them (I suspect it's as easy/easier to learn, but ultimately coroutines are part of Unity).

I typically decide to use Tasks for any game state altering code paths that I don't want an exception to mess up. Coroutines are something I might use for animating health bars or adding floating text on critical hits, though for more complicated nested sequences Tasks may still be more stable for code driven animation sequences (fun fact, Task was originally written in C++ for use with Cocos2dX and was called AnimationSequence until I realized it was a much more general scheduling system).

## Interface Description

### High Level

Task contains a few important methods.  Typically you'll want to attach a "RootTask" component to an item so you can see the heirarchy in the editor when you look at the game object in question, so we will assume that is done.  If you like, you can simply instantiate a root task node like so: var task = new Task(); and there is no Unity dependency there.

		RootTask state;

RootTask is only used for viewing task state visually in the unity inspector window.

There are two lists of child tasks in any given parent task.

1. **Sequential**: only one runs at any given time, when one finishes the next one starts up on the following update call.
2. **Parallel**: all of these run in sequence every update call.

If I have a task structure like this:

		root
			|**:Sequential Tasks:**
			|1 Countdown Timer
			|2 Blow Up
			|3 Fade Out
			|**:Parallel Tasks:**
			|- Update Player Position
			|- Update Enemy Position

it is pretty easy to visualize "Countdown Timer" completing, then moving on to the next sequential task "Blow Up", and finally "Fade Out", all the while "Update Player Position" and "Update Enemy Position" occur every frame regardless of what's going on in the sequential queue.

Now, consider each of those tasks could have additional child tasks, all of which must complete before the parent can call itself finished (or be marked "non-blocking" upon creation which means it won't stop the parent task from finishing, and will itself finish as the parent does.)

Let's visualize this with a graph before moving on to how to add new tasks to an existing one.

		root
			|**:Sequential Tasks:**
			|1 Countdown Timer
				|**:Sequential Tasks:**
				|1 Subtract Time
				|2 Update Text Field
			|2 Blow Up
			|3 Fade Out
			|**:Parallel Tasks:**
			|- Update Player Position
			|- Update Enemy Position

Now Countdown Timer has two child tasks (on top of its existing update method) each of which has to be complete before the second sequential task can run.

### Adding Items To A Task

There are several methods for adding children to a task, you can read these as commands which describe where in the queue a new task will appear:

#### Sequential:
1. **now**: Add an item to the *start* of a task's sequential list. If the previous item in that list was already running it will call onSuspend, and onResume for that displaced task. As you can tell, now indicates we want to add something that runs sequentially and should start "now".
2. **then**: Add an item to teh *end* of a task's sequential list. This is less disruptive and will simply append a new task to the existing list, it will run after all the previous items have completed.

#### Parallel:
3. **also**: Add an item to the end of a task's parallel list. This means the execution order or parallel is guaranteed in the order of being added, but all parallel tasks will run with each call to the parent task's update method.

#### Sequential Non-Blocking:
4. **thenAlso**: This is a weird duck. We only have Sequential and Parallel lists, so how does this work? We add a new task to the end of the current task's sequential list. When this task comes up in queue it is popped off the sequential list and appended to the parallel list. In this way we can sequence an item that should run at a specific time, but should not block items that show up after it.
5. ~~nowAlso~~: There is no such thing as nowAlso, this is simply "also".

All of these have similar signatures:

		now(String a_name, TaskAction a_task, bool a_blockParentCompletion = true)
		then(String a_name, TaskAction a_task, bool a_blockParentCompletion = true)
		thenAlso(String a_name, TaskAction a_task, bool a_blockParentCompletion = true)
		also(String a_name, TaskAction a_task, bool a_blockParentCompletion = true)

Each one takes an identifying name, a delegate with the signature: **bool F(Task self, double delta)**, and a flag to block parent completion or not. If you pass in false, it means that the child task will run until either its update loop returns true, or all other child actions are complete (or the parent is cancelled). By default all children will block their parent from finishing.

Let's talk briefly about the update method. Typically it'll look something like this:

		task.then("example", (Task self, double dt) => 
		{
			//Do some meaningful action on a per frame basis
			return trueWillSignalCompletion;
		});

This is essentially an update method. It gets called when the root update() method is called by the user and the child is sequenced and ready to go. Returning true will signal the local task (the one containing that update method) is complete, and it will allow that task's sequential children to run.

* *Note*: Sequential child items do not run UNTIL the local update method for that task returns true. Parallel items will run alongside the local update method, though. You can stop the local update task from blocking sequential child items so all children get updated regardless of the local update method finishing by calling **unblockChildTasks**.

If you call **finish** on a task it will set a flag and the task system will gracefully finish the next time update is called before running the user supplied update method, it will act as if the user supplied update method returned true, and will call onFinish.

If you call **cancel** on a task it will eject it and all children from the current sequence and call onCancel if the task was already in the process of running (it will silently remove non-started tasks though.) onCancel will be called.

#### Organizing Task Heirarchy

You'll notice a few more versions of now, then, thenAlso, and also:

		now(String a_name, bool a_blockParentCompletion = true)
		then(String a_name, bool a_blockParentCompletion = true)
		thenAlso(String a_name, bool a_infinite = false, bool a_blockParentCompletion = true)
		also(String a_name, bool a_infinite = false, bool a_blockParentCompletion = true)

These are primarily convenience methods for creating containers of stuff, mostly useful for debugging visually, or creating conceptual buckets of like things.

**now** and **then** both have no option to run infinitely. It doesn't make sense to queue up something blocking sequentially that will *never* complete. Basically these two methods are wrappers for: 

		now/then(a_name, delegate(Task task, double dt) { return true; }, a_blockParentCompletion);

Enough said. Simple parent label for a sequential bucket of tasks.

**thenAlso** and most notably **also** are more general buckets.  **thenAlso** is primarily included for completeness, but most often I find myself using something like this: 

		var creatureSpawnerRootTask = task.also("CreatureSpawner", true).last();

The concept here is if you turn on infinite it won't close when there are no child tasks, if infinite is false, it will close off the root task as soon as no further work is left for the children. Note, the implementation:

        public Task thenAlso(String a_name, bool a_infinite = false, bool a_blockParentCompletion = true)
        {
            var result = thenAlso(a_name, delegate(Task task, double dt) { return !a_infinite; }, a_blockParentCompletion);
            if (a_infinite)
            {
                result.last().unblockChildTasks();
            }
            return result;
        }

The end effect of calling this is a reliable task bucket that will stick around until you cancel or finish it manually.

#### .last() and chainable syntax

Finally, you'll notice calls to .last() when I want to actually refer to the current task I made. Why is this? **now, then, thenAlso, and also** return references to the current level task *not* to the newly created task.  This is to enable chainable syntax:

		task.
			now("dance", (Task, double){Debug.Log("dance");return true;}).
			then("laugh", (Task, double){Debug.Log("laugh");return true}).
			then("joke", (Task, double){Debug.Log("joke");return true;}).
			also("beHappy", (Task, double){Debug.Log("I am Happy!");return false;}, false);

This would create an awfully deep completely useless heirarchy if we returned the child task from each. I'll post a comparison of each heirarchy:

		//Actual Heirarchy
		root
			|:**Sequential**:
			|1 dance
			|2 laugh
			|3 joke
			|:**Parallel**:
			|- beHappy

		//Hypothetical Heirarchy If Implemented Differently
		root
			|:**Sequential**:
			|1 dance
				|:**Sequential**:
				|1 laugh
					|:**Sequential**:
					|1 joke
						|:**Parallel**:
						|- beHappy

So, if you wanted to access each one, you need to call .last() which returns the most recently added new task.

		var danceTask = task.now("dance", (Task, double){Debug.Log("dance");return true;}).last();
		var laughTask = task.then("laugh", (Task, double){Debug.Log("laugh");return true}).last();
		var jokeTask = task.then("joke", (Task, double){Debug.Log("joke");return true;}).last();
		var beHappyTask = task.also("beHappy", (Task, double){Debug.Log("I am Happy!");return false;}, false).last();

##ActionBase

In order to facilitate reusable concepts you can derive from ActionBase to eliminate some of the callback boilerplate and offer your tasks more structure. Where the examples above all have the lambda logic embedded, passing in a premade class can more succinctly communicate your intent.

Take a look at TaskActions.cs for some examples of premade behaviours and feel free to invent your own! Making use of them is as easy as calling task.then(BlockForSeconds(1.25f)); for example!

And that's it!
