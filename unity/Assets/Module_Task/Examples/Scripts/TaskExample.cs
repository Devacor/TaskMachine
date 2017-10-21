using System.Collections;
using UnityEngine;
using UnityEngine.UI;

namespace SharedCore.Example
{
    public class TaskExample : MonoBehaviour
    {
        [SerializeField] private GameObject bullet;

        [SerializeField] private Text counter;

        [SerializeField] private readonly float counterDuration = 10.0f;

        [SerializeField] private Transform DropPosition;

        [SerializeField] private GameObject explosion;

        private Vector3 explosionScale;

        [SerializeField] private Transform FinishPosition;

        [SerializeField] private Transform LandPosition;

        [SerializeField] private readonly float resetWaitTime = 3.0f;

        [SerializeField] private readonly float shootDuration = 2.0f;

        [SerializeField] private RootTask state = null;


        [SerializeField]
        private GameObject bulletCo;

        [SerializeField]
        private Text counterCo;

        [SerializeField]
        private Transform DropPositionCo;

        [SerializeField]
        private GameObject explosionCo;

        [SerializeField]
        private Transform FinishPositionCo;

        [SerializeField]
        private Transform LandPositionCo;

        // Use this for initialization
        private void Start()
        {
            explosionScale = explosion.transform.localScale;

            InitiateTask();
            StartCoroutine(EquivalentCoroutine());
        }

        //49 LINES
        private IEnumerator EquivalentCoroutine()
        {
            while (true)
            {
                //Countdown
                var countdownTime = 0.0f;
                var lastFullSecond = -1;
                while (countdownTime < counterDuration)
                {
                    countdownTime = Mathf.Min(countdownTime + Time.smoothDeltaTime, counterDuration);
                    if (lastFullSecond != (int)countdownTime) //only update text when it changes.
                    {
                        lastFullSecond = (int)countdownTime;
                        counterCo.text = "CountDown Coroutine: " + Mathf.Round(counterDuration - countdownTime);
                    }
                    bulletCo.transform.position = Vector3.Lerp(DropPositionCo.position, LandPositionCo.position,
                        countdownTime / counterDuration);
                    yield return new WaitForEndOfFrame();
                }

                //Shoot
                countdownTime = 0.0f;
                while (countdownTime < shootDuration)
                {
                    countdownTime = Mathf.Min(countdownTime + Time.smoothDeltaTime, shootDuration);
                    bulletCo.transform.position = Vector3.Lerp(LandPositionCo.position, FinishPositionCo.position,
                        countdownTime / shootDuration);
                    yield return new WaitForEndOfFrame();
                }

                //Explode
                explosionCo.SetActive(true);
                while (explosionCo.transform.localScale != Vector3.one)
                {
                    var scaledAmount = 1.0f * Time.smoothDeltaTime;
                    explosionCo.transform.localScale -= new Vector3(scaledAmount, scaledAmount, scaledAmount);
                    if (explosionCo.transform.localScale.x <= 1.0f)
                    {
                        explosionCo.transform.localScale = Vector3.one;
                    }
                    yield return new WaitForEndOfFrame();
                }
                explosionCo.transform.localScale = explosionScale;
                bulletCo.SetActive(false);
                explosionCo.SetActive(false);

                //WaitToReset
                yield return new WaitForSeconds(resetWaitTime);

                bulletCo.SetActive(true);
                bulletCo.transform.position = DropPositionCo.position;
            }
        }


        /*
		 * Set up this structure:
		 * |-Countdown
		 *	  |-UpdateText
		 *	  |+DropBall
		 * |-Shoot
		 * |-Explode
		 * |-WaitToReset
		 * 
		 * 50 LINES
		*/

        private void InitiateTask()
        {
            state.task.then("Countdown").recent()
                .also("DropBall", (Task self, double dt) =>
                {
                    bullet.transform.position = Vector3.Lerp(DropPosition.position, LandPosition.position,
                        Mathf.Min((float)self.elapsed(), counterDuration) / counterDuration);
                    return (float)self.elapsed() >= counterDuration;
                })
                .then("UpdateText", (Task self, double dt) =>
                {
                    counter.text = "CountDown Task: " +
                                   Mathf.Round(counterDuration - Mathf.Min((float)self.elapsed(), counterDuration));
                    return (float)self.elapsed() >= counterDuration;
                }).recent().localInterval(1.0f);
            //Note the interval which applies to UpdateText so it only updates when it changes.

            var explodeTask = state.task.then("Shoot", (Task self, double dt) =>
            {
                bullet.transform.position = Vector3.Lerp(LandPosition.position, FinishPosition.position,
                    Mathf.Min((float)self.elapsed(), shootDuration) / shootDuration);
                return (float)self.elapsed() >= shootDuration;
            }).then("Explode", (Task self, double dt) =>
            {
                var scaledAmount = 1.0f * (float)dt;
                explosion.transform.localScale -= new Vector3(scaledAmount, scaledAmount, scaledAmount);
                if (explosion.transform.localScale.x <= 1.0f)
                {
                    explosion.transform.localScale = Vector3.one;
                }
                return explosion.transform.localScale == Vector3.one;
            }).recent();
            explodeTask.onStart += (Task self) => { explosion.SetActive(true); };
            explodeTask.onFinish += (Task self) =>
            {
                explosion.transform.localScale = explosionScale;
                bullet.SetActive(false);
                explosion.SetActive(false);
            };

            var resetTask = state.task.then(new BlockForSeconds(resetWaitTime)).recent();
            resetTask.onFinish += (Task self) =>
            {
                bullet.SetActive(true);
                bullet.transform.position = DropPosition.position;
                InitiateTask();
            };
        }

        // Update is called once per frame
        private void Update()
        {
            state.task.update(Time.smoothDeltaTime);
        }
    }
}