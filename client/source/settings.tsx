import { Component, Match, Switch, createSignal } from "solid-js";
import { Form, Row, Col, Button } from "solid-bootstrap";

import MainModuleFactory, * as Wrapper from "../wrapper/binary/wrapper"

/*MainModuleFactory().then(wrapper =>
{
    let a : Wrapper.SessionCreateForm = 
    {
        mesh_generator: wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE         
    };

    wrapper.build_session_create_packet(a);
})*/

const NumberSetting : Component<{label : string, step : number, min : number, max : number}> = (props) =>
{
    return (
        <Form.Group as={Row} class="mb-3">
            <Form.Label as="legend" column sm={4}>
                {props.label}
            </Form.Label>
            <Col sm={8}>
                <Form.Control type="number" step={props.step} min={props.min} max={props.max}></Form.Control>
            </Col>
        </Form.Group>
    )
};

const GeneralSettings : Component = () =>
{
    return (
        <div>
            <div class="border-bottom mb-3">
                <h5>General</h5>
            </div>
            <div class="px-2">
                <NumberSetting label={"Resolution"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Update Rate"} step={1} min={256} max={1024}></NumberSetting>
            </div>
        </div>
    )
};

const SceneSettings : Component = () =>
{
    return (
        <div>
            <div class="border-bottom mb-3">
                <h4>Scene</h4>
            </div>
            <div class="px-2">
                <NumberSetting label={"Scale"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Exposure"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Indirect Intensity"} step={1} min={256} max={1024}></NumberSetting>
            </div>
        </div>
    )
};

const VideoSettings : Component = () =>
{
    return (
        <div>
            <div class="border-bottom mb-3">
                <h4>Video</h4>
            </div>
            <div class="px-2">
                <NumberSetting label={"Chroma-Subsampling"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Mode"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Frame Rate"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Bitrate Rate"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Quality"} step={1} min={256} max={1024}></NumberSetting>
            </div>
        </div>
    )
};


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

const Settings : Component<{set_state : (state : string) => void}> = (props) =>
{
    const [method, set_method] = createSignal("line");

    return (
        <div>
            <Form>
                <GeneralSettings></GeneralSettings>
                <SceneSettings></SceneSettings>
                <VideoSettings></VideoSettings>


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
            <div class="d-flex">
                <Button class="me-auto" onClick={() => props.set_state("scene")}>Back</Button>
                <Button onClick={() => props.set_state("capture")}>Next</Button>
            </div>
        </div>
    )
};

export default Settings;