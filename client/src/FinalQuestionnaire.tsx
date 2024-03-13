import { DragDropProvider, DragDropSensors, DragEvent, DragOverlay, Id, SortableProvider, closestCenter, createSortable, useDragDropContext } from '@thisbeyond/solid-dnd';
import { Button, Form, FormLabel, ListGroup, FloatingLabel, Spinner, Modal } from 'solid-bootstrap';
import { type Component, createSignal, For, Show, createMemo, createEffect, Switch, Match } from 'solid-js';
import styles from './FinalQuestionnaire.module.css';
import { SERVER_URL } from './App';
import { useNavigate } from '@solidjs/router';
import { useLogout } from './WithParticipantId';

const Condition: Component<{
    id: Id,
}> = ({ id }) => {
    return (
        <ListGroup.Item
            class="sortable"
        >
            <big>Condition {id}</big>
        </ListGroup.Item>
    );
};

const Item: Component<{
    id: Id,
}> = ({ id }) => {
    const sortable = createSortable(id);
    const context = useDragDropContext();
    const state = context?.[0];

    return (
        <div
            use:sortable
            style={{
                opacity: sortable.isActiveDraggable ? 0.25 : 1.0,
                cursor: 'grab',
                'user-select': 'none'
            }}
        >
            <Condition id={id} />
        </div>
    );
};

export const ConditionOrder: Component<{
    ids: () => Id[],
    setIds: (ids: Id[]) => void,
    disabled?: boolean,
}> = (props) => {
    const [activeItem, setActiveItem] = createSignal<Id>();

    const onDragStart = (event: DragEvent) => setActiveItem(event.draggable.id);

    const onDragEnd = ({ draggable, droppable }: DragEvent) => {
        if (draggable && droppable) {
            const currentIds = props.ids();
            const fromIndex = currentIds.indexOf(draggable.id);
            const toIndex = currentIds.indexOf(droppable.id);
            if (fromIndex !== toIndex) {
                const updatedIds = currentIds.slice();
                updatedIds.splice(toIndex, 0, ...updatedIds.splice(fromIndex, 1));
                props.setIds(updatedIds);
            }
        }
    };

    createEffect(() => {
        console.log(props.disabled);
    });

    return (
        <Switch>
            <Match when={props.disabled}>
                <ListGroup>
                    <For each={props.ids()}>
                    {
                        id => <Condition id={id} />
                    }
                    </For>
                </ListGroup>
            </Match>
            <Match when={!props.disabled}>
                <DragDropProvider
                    onDragStart={onDragStart}
                    onDragEnd={onDragEnd}
                    collisionDetector={closestCenter}
                >
                    <DragDropSensors />
                    <ListGroup>
                        <SortableProvider ids={props.ids()}>
                            <For each={props.ids()}>
                            {
                                id => <Item id={id} />
                            }
                            </For>
                        </SortableProvider>
                    </ListGroup>
                    <DragOverlay>
                        <Show when={activeItem()}>
                        {
                            id => <Condition id={id()} />
                        }
                        </Show>
                    </DragOverlay>
                </DragDropProvider>
            </Match>
        </Switch>
    );
};

type Gender = "male" | "female" | "other";
type Knowledge = 'none' | 'basic' | 'knowledgable' | 'expert';
type State = "entry" | "submitting" | "submitted";

const FinalQuestionnaire: Component<{
    participantId: number,
}> = ({ participantId }) => {
    const [ids, setIds] = createSignal<Id[]>([1, 2, 3]);
    const [age, setAge] = createSignal<number>();
    const [gender, setGender] = createSignal<Gender>();
    const [knowledge, setKnowledge] = createSignal<Knowledge>();
    const [profession, setProfession] = createSignal<string>();
    const [feedback, setFeedback] = createSignal<string>();
    const [state, setState] = createSignal<State>("entry");
    const [submitted, setSubmitted] = createSignal<boolean>();
    const navigate = useNavigate();
    const logout = useLogout();

    const formComplete = createMemo(() =>
        age() !== undefined &&
        gender() !== undefined &&
        knowledge() !== undefined &&
        profession() !== undefined
    );
    const value = createMemo(() => ({
        order: ids(),
        age: age(),
        gender: gender(),
        knowledge: knowledge(),
        profession: profession(),
        feedback: feedback(),
    }));

    fetch(`${SERVER_URL}/final?participantId=${participantId}`)
        .then(async response => {
            setSubmitted(response.ok);
            if (response.ok) {
                const json = await response.json();
                setIds(json.order)
                setAge(json.age)
                setGender(json.gender)
                setKnowledge(json.knowledge)
                setProfession(json.profession)
                setFeedback(json.feedback)
            }
        });

    return (
        <div>
            <h1>Final Questionnaire</h1>
            <Form>
                <Form.Group class={styles.formGroup}>
                    <FormLabel>Please sort the overall feel of the condition from best to worst via drag & drop</FormLabel>
                    <div class={styles.conditionOrderGroup}>
                        <div class={styles.conditionOrderLegend}>
                            <span>Best</span>
                            <span>Worst</span>
                        </div>
                        <div class={styles.conditionOrder}>
                            <ConditionOrder ids={ids} setIds={setIds} disabled={submitted()} />
                        </div>
                    </div>
                </Form.Group>

                <Form.Group class={styles.formGroup}>
                    <FloatingLabel label="Age">
                        <Form.Control
                            type="number"
                            placeholder="123"
                            inputmode="numeric"
                            pattern="\d*"
                            size="sm"
                            isInvalid={typeof age() !== "number"}
                            disabled={submitted()}
                            onKeyDown={event => {
                                switch (event.key) {
                                    case '0':
                                    case '1':
                                    case '2':
                                    case '3':
                                    case '4':
                                    case '5':
                                    case '6':
                                    case '7':
                                    case '8':
                                    case '9':
                                    case 'Backspace':
                                    case 'Delete':
                                        break;
                                    default:
                                        event.preventDefault();
                                        event.stopPropagation();
                                }
                            }}
                            value={age()}
                            onInput={event => {
                                const value = parseInt(event.currentTarget.value);
                                if (!Number.isNaN(value)) {
                                    setAge(value);
                                } else {
                                    setAge();
                                }
                            }}
                        />
                    </FloatingLabel>
                </Form.Group>

                <Form.Group class={styles.formGroup}>
                    <FloatingLabel label="Gender">
                        <Form.Select
                            isInvalid={typeof gender() !== "string"}
                            value={gender()}
                            disabled={submitted()}
                            onChange={event => {
                                const value = event.currentTarget.value;
                                switch (value) {
                                    case 'female':
                                    case 'male':
                                    case 'other':
                                        setGender(value);
                                        break;

                                    default:
                                        setGender();
                                }
                            }}
                        >
                            <option value="">Please choose</option>
                            <option value="female">Female</option>
                            <option value="male">Male</option>
                            <option value="other">Other</option>
                        </Form.Select>
                    </FloatingLabel>
                </Form.Group>

                <Form.Group class={styles.formGroup}>
                    <FloatingLabel label="Profession (In case of student, please specify course of study)">
                        <Form.Control
                            type="text"
                            size="sm"
                            disabled={submitted()}
                            placeholder="Profession"
                            isInvalid={typeof profession() !== "string"}
                            value={profession()}
                            onInput={event => {
                                const value = event.currentTarget.value.trim();
                                if (value.length > 0) {
                                    setProfession(value);
                                } else {
                                    setProfession();
                                }
                            }}
                        />
                    </FloatingLabel>
                </Form.Group>

                <Form.Group class={styles.formGroup}>
                    <FloatingLabel label="Knowledge in the Field of Rendering (Own Judgement)">
                        <Form.Select
                            isInvalid={typeof knowledge() !== "string"}
                            value={knowledge()}
                            disabled={submitted()}
                            onChange={event => {
                                const value = event.currentTarget.value;
                                switch (value) {
                                    case 'none':
                                    case 'basic':
                                    case 'knowledgable':
                                    case 'expert':
                                        setKnowledge(value);
                                        break;

                                    default:
                                        setKnowledge();
                                }
                            }}
                        >
                            <option value="">Please choose</option>
                            <option value="none">None</option>
                            <option value="basic">Basic Knowledge</option>
                            <option value="knowledgable">Knowledgable</option>
                            <option value="expert">Expert</option>
                        </Form.Select>
                    </FloatingLabel>
                </Form.Group>

                <Form.Group class={styles.formGroup}>
                    <FloatingLabel label="Additional Feedback">
                        <Form.Control
                            as="textarea"
                            disabled={submitted()}
                            placeholder="Additional feedback"
                            style={{ height: '100px' }}
                            value={feedback()}
                            onInput={event => {
                                setFeedback(event.currentTarget.value);
                            }}
                        />
                    </FloatingLabel>
                </Form.Group>

                <Button
                    variant="primary"
                    type="submit"
                    disabled={!formComplete() || state() !== "entry" || submitted()}
                    onClick={async event => {
                        setState("submitting");
                        event.preventDefault();

                        const response = await fetch(
                            `${SERVER_URL}/final?participantId=${participantId}`,
                            {
                                method: "POST",
                                headers: {
                                  "Content-Type": "application/json",
                                },
                                body: JSON.stringify(value()),
                            }
                        );
                        if (response.ok) {
                            setState("submitted");
                        }
                    }}
                >
                    <Switch>
                        <Match when={submitted()}>
                            Already submitted
                        </Match>
                        <Match when={!submitted() && state() === "submitting"}>
                            Submitting&nbsp;<Spinner animation="border" size="sm" />
                        </Match>
                        <Match when={!submitted() && state() !== "submitting"}>
                            Submit
                        </Match>
                    </Switch>
                </Button>
            </Form>
            <Modal show={state() === "submitted"}>
                <Modal.Body>
                    Thank you for participating in our study, please take a snack. 😊
                </Modal.Body>
                <Modal.Footer>
                    <Button
                        onClick={() => {
                            if (logout) {
                                logout();
                            }
                            navigate("/");
                        }}
                    >
                        Finish
                    </Button>
                </Modal.Footer>
            </Modal>
        </div>
    );
};

export default FinalQuestionnaire;
