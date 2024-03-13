import { createSignal, type Component } from 'solid-js';
import { Button, FloatingLabel, Form, Modal } from 'solid-bootstrap';

const ParticipantIdForm: Component<{ setParticipantId: (paricipantId: number) => void }> = ({ setParticipantId }) => {
    const [currentParticipantId, setCurrentParticipantId] = createSignal<number>();

    return (
        <Modal show={true}>
            <Modal.Body>
                <Form>
                    <FloatingLabel controlId="floatingInput" label="Participant ID" class="mb-3">
                        <Form.Control
                            type="number"
                            placeholder="123"
                            inputmode="numeric"
                            pattern="\d*"
                            size="lg"
                            autofocus
                            isInvalid={typeof currentParticipantId() !== "number"}
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
                                    case 'Enter':
                                        break;
                                    default:
                                        console.log(event.key);
                                        event.preventDefault();
                                        event.stopPropagation();
                                }
                            }}
                            onInput={event => {
                                const value = parseInt(event.currentTarget.value);
                                if (!Number.isNaN(value) && value < 10000) {
                                    setCurrentParticipantId(value);
                                } else {
                                    setCurrentParticipantId();
                                }
                            }}
                        />
                    </FloatingLabel>
                    <Button
                        variant="primary"
                        type="submit"
                        disabled={typeof currentParticipantId() !== "number"}
                        onClick={event => {
                            const particpantId = currentParticipantId();
                            if (typeof particpantId === 'number') {
                                setParticipantId(particpantId);
                            }
                            event.preventDefault();
                            event.stopPropagation();
                        }}
                    >
                        Start
                    </Button>
                </Form>
            </Modal.Body>
        </Modal>
    );
};

export default ParticipantIdForm;
