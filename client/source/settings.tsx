import { Component, Match, Switch, createSignal } from "solid-js";
import { Form, Row, Col } from "solid-bootstrap";


const LayerSettings : Component = () =>
{


    return (
        <a></a>
    )
};

const LineSettings : Component = () =>
{



    return (
        <div>
            <Form.Group as={Row} class="mb-2">
                <Form.Label as="legend" column sm={2}>
                    Laplace Threshold
                </Form.Label>
                <Col sm={10}>
                    <Form.Control type="number" step="0.01" min="0.0" max="10.0"></Form.Control>
                </Col>
            </Form.Group>
            <Form.Group as={Row} class="mb-2">
                <Form.Label as="legend" column sm={2}>
                    Normal Scale
                </Form.Label>
                <Col sm={10}>
                    <Form.Control type="number" step="0.01" min="0.0" max="10.0"></Form.Control>
                </Col>
            </Form.Group>
            <Form.Group as={Row} class="mb-2">
                <Form.Label as="legend" column sm={2}>
                    Line Length Min
                </Form.Label>
                <Col sm={10}>
                    <Form.Control type="number" step="0.01" min="0.0" max="10.0"></Form.Control>
                </Col>
            </Form.Group>
        </div>
    );
};

const LoopSettings : Component = () =>
{
    return (
        <a></a>
    )
};

const Settings : Component = () =>
{
    const [method, set_method] = createSignal("line");

    return (
        <Form class="p-4">
            <Form.Select class="mb-4" onChange={(event) => set_method(event.target.value)}>
                <option value="line">Line Based</option>
                <option value="loop">Loop Based</option>
            </Form.Select>
            <Switch>
                <Match when={method() == "line"}>
                    <LineSettings></LineSettings>
                </Match>
            </Switch>
        </Form>
    )
};

export default Settings;