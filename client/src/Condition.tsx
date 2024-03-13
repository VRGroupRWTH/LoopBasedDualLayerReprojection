import { Button, Spinner } from 'solid-bootstrap';
import { createSignal, type Component, Show, Switch, Match } from 'solid-js';
import RemoteRendering from './RemoteRendering';
import { SERVER_URL } from './App';
import { useNavigate } from '@solidjs/router';
import image from './assets/vase_preview.png';

type ConditionStatus = "init" | "running" | "finished";

const Condition: Component<{
    participantId: number,
    condition: number,
    trial?: boolean,
}> = ({ participantId, condition, trial }) => {
    const currentId = trial ? `cond${condition}-trial` : `cond${condition}-run`;
    const nextId = trial ? `cond${condition}-run` : `cond${condition}-questionnaire`;
    const [status, setStatus] = createSignal<ConditionStatus>("init");
    const navigate = useNavigate();
    const [completed, setCompleted] = createSignal<boolean>();
    
    const onFinished = async (params: any) => {
        setStatus("finished");

        const response = await fetch(
            `${SERVER_URL}/${currentId}?participantId=${participantId}`,
            {
                method: "POST",
                headers: {
                  "Content-Type": "application/json",
                },
                body: JSON.stringify(params),
            }
        );
        if (response.ok) {
            window.location.href = `/interactive-3d-streaming/${nextId}`;
            // navigate(`/${nextId}`);
        }
    };

    fetch(`${SERVER_URL}/progress?participantId=${participantId}`)
        .then(async response => {
            if (response.ok) {
                const json = await response.json();
                setCompleted(json[currentId] === true);
            }
        });

    return (
        <Switch>
            <Match when={status() === "init"}>
                <h1>
                    Condition {condition}
                    <Show when={trial}>
                        &nbsp;-&nbsp;
                        <small>Trial Run</small>
                    </Show>
                </h1>
                <Switch>
                    <Match when={condition === 1 && trial}>
                        <p>
                            In this study, you can explore a virtual world using this tablet.
                            When you look "through" the tablet you will see a virtual bistro.
                            Your goal is to find to find <b>5</b> vases (as shown below) in this bistro.
                            Each vase has a number from 0 to 9 written on it.
                            Whenever you find a vase, select the corresponding number from the bottom of the screen.
                            When all vases present in the bistro are marked, the task is completed.
                            If any vase is marked that is currently not present in the bistro the task cannot be completed.
                        </p>
                        <div style={{ display: "flex", "flex-direction": "row", "justify-content": "center" }}>
                            <img src={image} style={{ "width": "400px", "margin-bottom": "1em" }} />
                        </div>
                        <p>
                            The task will be repeated three times for different conditions.
                            Before each repitition you can make yourself familiar with the system in a trial run where you can take your time.
                            After the trial run the search task starts and you should try to find all vases as quickly as possible.
                            After each condition you will fill out a short questionnaire on this tablet before continuing with the next condition.
                            At the end there will be an additional questionnaire covering all conditions.
                        </p>
                        <p>
                            You do not need to remember everything as you will be guided through the study.
                            If you are ready to enter the <b>trial</b> run for <b>Condition 1</b> press Start below.
                        </p>
                    </Match>
                    <Match when={condition !== 1 && trial}>
                        <p>
                            You are about to enter the trial run for <b>Condition {condition}</b> you can take your time.
                        </p>
                    </Match>
                    <Match when={!trial}>
                        <p>
                            You are about to enter the timed run for <b>Condition {condition}</b>, try to find all <b>5</b> vases as quickly as possible.
                        </p>
                    </Match>
                </Switch>
                <Button
                    disabled={completed()}
                    onClick={() => setStatus("running")}
                >
                    <Switch>
                        <Match when={completed() === undefined}>
                            <Spinner animation="border" />
                        </Match>
                        <Match when={completed() === true}>
                            Already completed
                        </Match>
                        <Match when={completed() === false}>
                            Start
                        </Match>
                    </Switch>
                </Button>
            </Match>
            <Match when={status() === "running"}>
                <RemoteRendering finished={onFinished} participantId={participantId} condition={condition} trial={trial} />
            </Match>
            <Match when={status() === "finished"}>
                <Spinner animation="border" />
            </Match>
        </Switch>
    );
};

export default Condition;
