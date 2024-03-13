import { Button, Form, Spinner, FloatingLabel } from 'solid-bootstrap';
import { createSignal, type Component, createMemo, Show, Switch, Match } from 'solid-js';
import styles from './NasaTLXQuestionnaire.module.css';
import { SERVER_URL } from './App';
import { useNavigate } from '@solidjs/router';

const Scale: Component<{
    getter: () => number | undefined,
    setter: (value: number) => void,
    name: string,
    description: string,
    minText?: string,
    maxText?: string,
    ticks?: number,
    disabled?: boolean,
}> = ({ name, description, minText, maxText, getter, setter, ticks, disabled, }) => {
    return (
        <Form.Group class={styles.formGroup}>
            <div class={styles.distribute}>
                <Form.Label>{name}</Form.Label>
                <Form.Text class="text-muted">{description}</Form.Text>
            </div>
            <Form.Range
                min={0}
                value={getter() || (ticks || 20) / 2}
                max={ticks || 20}
                style={{
                    opacity: typeof getter() === 'number' ? 1.0 : 0.5,
                }}
                onChange={event => {
                    setter(parseInt(event.currentTarget.value));
                }}
                disabled={disabled}
            />
            <div class={styles.distribute}>
                <Form.Text class="text-muted">{ minText || "Very Low" }</Form.Text>
                <Form.Text class="text-muted">{ maxText || "Very High" }</Form.Text>
            </div>
        </Form.Group>
    );
};

type Frequency = 'often' | 'sometimes' | 'seldom' | 'never';

const FrequencySelector: Component<{
    getter: () => Frequency | undefined,
    setter: (value: Frequency|undefined) => void,
    name: string,
    description: string,
    disabled?: boolean,
}> = ({ getter, setter, name, description, disabled }) => {
    return (
        <Form.Group class={styles.formGroup}>
            <div class={styles.distribute}>
                <Form.Label>{name}</Form.Label>
                <Form.Text class="text-muted">{description}</Form.Text>
            </div>
            <Form.Select
                disabled={disabled}
                isInvalid={typeof getter() !== "string"}
                value={getter()}
                onChange={event => {
                    const value = event.currentTarget.value;
                    switch (value) {
                        case 'often':
                        case 'sometimes':
                        case 'seldom':
                        case 'never':
                            setter(value);
                            break;

                        default:
                            setter(undefined);
                    }
                }}
            >
                <option value="">Please choose</option>
                <option value="never">Never</option>
                <option value="seldom">Seldom</option>
                <option value="sometimes">Sometimes</option>
                <option value="often">Often</option>
            </Form.Select>
        </Form.Group>
    );
}

interface NasaTLXQuestionnaireResult {
    mentalDemand: number,
    physicalDemand: number,
    temporalDemand: number,
    performance: number,
    effort: number,
    frustration: number,

    blurryness: Frequency,
    stuttering: Frequency,
    blackPixels: Frequency,
    wrongColors: Frequency,
    deformedObjects: Frequency,
}

const NasaTLXQuestionnaire: Component<{
    participantId: number,
    id: string,
    action: string,
}> = ({ participantId, id, action }) => {
    const [mentalDemand, setMentalDemand] = createSignal<number>()
    const [physicalDemand, setPhysicalDemand] = createSignal<number>()
    const [temporalDemand, setTemporalDemand] = createSignal<number>()
    const [performance, setPerformance] = createSignal<number>()
    const [effort, setEffort] = createSignal<number>()
    const [frustration, setFrustration] = createSignal<number>();

    const [blurry, setBlurry] = createSignal<Frequency>();
    const [stuttering, setStuttering] = createSignal<Frequency>();
    const [blackPixels, setBlackPixels] = createSignal<Frequency>();
    const [wrongColors, setWrongColors] = createSignal<Frequency>();
    const [deformedObjects, setDeformedObjects] = createSignal<Frequency>();

    const [submitting, setSubmitting] = createSignal(false);
    const [completed, setCompleted] = createSignal<boolean>();
    const navigate = useNavigate();

    const result = createMemo(() => {
        const md = mentalDemand();
        const pd = physicalDemand();
        const td = temporalDemand();
        const p = performance();
        const e = effort();
        const f = frustration();

        const b = blurry();
        const s = stuttering();
        const bP = blackPixels();
        const wC = wrongColors();
        const dO = deformedObjects();

        if (typeof md === 'number' &&
            typeof pd === 'number' &&
            typeof td === 'number' &&
            typeof p === 'number' &&
            typeof e === 'number' &&
            typeof f === 'number' &&

            typeof b === 'string' &&
            typeof s === 'string' &&
            typeof bP === 'string' &&
            typeof wC === 'string' &&
            typeof dO === 'string') {
            return {
                mentalDemand: md,
                physicalDemand: pd,
                temporalDemand: td,
                performance: p,
                effort: e,
                frustration: f,
                blurryness: b,
                stuttering: s,
                blackPixels: bP,
                wrongColors: wC,
                deformedObjects: dO,
            } as NasaTLXQuestionnaireResult;
        } else {
            return undefined;
        }
    });

    fetch(`${SERVER_URL}${id}?participantId=${participantId}`)
        .then(async response => {
            setCompleted(response.ok);
            if (response.ok) {
                const json = await response.json() as NasaTLXQuestionnaireResult;
                setMentalDemand(json.mentalDemand);
                setPhysicalDemand(json.physicalDemand);
                setTemporalDemand(json.temporalDemand);
                setPerformance(json.performance);
                setEffort(json.effort);
                setFrustration(json.frustration);
                setBlurry(json.blurryness);
                setStuttering(json.stuttering);
                setBlackPixels(json.blackPixels);
                setWrongColors(json.wrongColors);
                setDeformedObjects(json.deformedObjects);
            }
        });

    return (
        <Form>
            <Show when={completed() !== undefined}>
                <Scale
                    name="Mental Demand"
                    description="How mentally demanding was the task?"
                    getter={mentalDemand}
                    setter={setMentalDemand}
                    disabled={completed()}
                />
                <Scale
                    name="Physical Demand"
                    description="How physically demanding was the task?"
                    getter={physicalDemand}
                    setter={setPhysicalDemand}
                    disabled={completed()}
                />
                <Scale
                    name="Temporal Demand"
                    description="How hurried or rushed was the pace of the task?"
                    getter={temporalDemand}
                    setter={setTemporalDemand}
                    disabled={completed()}
                />
                <Scale
                    name="Performance"
                    description="How successful were you in accomplishing what you were asked to do?"
                    getter={performance}
                    setter={setPerformance}
                    minText="Perfect"
                    maxText="Failure"
                    disabled={completed()}
                />
                <Scale
                    name="Effort"
                    description="How hard did you have to work to accomplish your level of performance?"
                    getter={effort}
                    setter={setEffort}
                    disabled={completed()}
                />
                <Scale
                    name="Frustration"
                    description="How insecure, discouraged, irritated, stressed, and annoyed were you?"
                    getter={frustration}
                    setter={setFrustration}
                    disabled={completed()}
                />

                <FrequencySelector
                    name="Sharpness"
                    description="How often was the image blurry?"
                    getter={blurry}
                    setter={setBlurry}
                    disabled={completed()}
                />
                <FrequencySelector
                    name="Smoothness of Movement"
                    description="How often did the movement not feel smooth"
                    getter={stuttering}
                    setter={setStuttering}
                    disabled={completed()}
                />
                <FrequencySelector
                    name="Black Pixels"
                    description="How often did you notice black pixels?"
                    getter={blackPixels}
                    setter={setBlackPixels}
                    disabled={completed()}
                />
                <FrequencySelector
                    name="Wrong Colors"
                    description="How often did you notice wrongly colored objects?"
                    getter={wrongColors}
                    setter={setWrongColors}
                    disabled={completed()}
                />
                <FrequencySelector
                    name="Deformed Objects"
                    description="How often did you notice deformed objects?"
                    getter={deformedObjects}
                    setter={setDeformedObjects}
                    disabled={completed()}
                />

                <Button
                    variant="primary"
                    type="submit"
                    disabled={!result() || submitting() || completed() === true}
                    onClick={async event => {
                        setSubmitting(true);
                        const r = result();
                        event.preventDefault();
                        event.stopPropagation();
                        if (r) {
                            const response = await fetch(
                                `${SERVER_URL}${id}?participantId=${participantId}`,
                                {
                                    method: "POST",
                                    headers: {
                                      "Content-Type": "application/json",
                                    },
                                    body: JSON.stringify(r),
                                }
                            );
                            if (response.ok) {
                                navigate(action);
                            }
                        }
                    }}
                >
                    <Switch>
                        <Match when={completed()}>
                            Already submitted
                        </Match>
                        <Match when={!completed() && submitting()}>
                            Submitting&nbsp;<Spinner animation="border" size="sm" />
                        </Match>
                        <Match when={!completed() && !submitting()}>
                            Submit
                        </Match>
                    </Switch>
                </Button>
            </Show>
        </Form>
    );
};

export default NasaTLXQuestionnaire;
