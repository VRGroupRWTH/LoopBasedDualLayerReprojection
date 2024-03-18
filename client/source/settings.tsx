import { Component, Match, Switch, createSignal } from "solid-js";
import { Form, Row, Col, Button } from "solid-bootstrap";
import * as Wrapper from "../wrapper/binary/wrapper"

export type SettingsType = 
{
    resolution: number,
    update_rate: number,
    scene_scale: number,
    scene_exposure: number,
    scene_indirect_intensity: number
    mesh_generator: Wrapper.MeshGeneratorType,
    mesh_settings: Wrapper.MeshSettingsForm,
    video_settings: Wrapper.VideoSettingsForm
};




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
                <NumberSetting label={"Scene Scale"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Scene Exposure"} step={1} min={256} max={1024}></NumberSetting>
                <NumberSetting label={"Scene Indirect Intensity"} step={1} min={256} max={1024}></NumberSetting>
            </div>
        </div>
    )
};

const VideoSettings : Component = () =>
{
    return (
        <div>
            <div class="border-bottom mb-3">
                <h5>Video</h5>
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
        <div>
            <div class="border-bottom mb-3">
                <h5>Layer</h5>
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

const MeshSettings : Component = () =>
{
    return (
        <div>
            <div class="border-bottom mb-3">
                <h5>Mesh</h5>
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

export type SettingsProps =
{
    wrapper : Wrapper.MainModule,
    settings : () => SettingsType,
    set_state : (state : string) => void,
    set_settings : (settings : SettingsType) => void
};

export const Settings : Component<SettingsProps> = (props) =>
{
    return (
        <div>
            <Form>
                <div class="border-bottom mb-3"><h5>General</h5></div>
                <div class="px-2">
                    <NumberSetting name="" label={"Resolution"} step={1} min={256} max={1024}></NumberSetting>
                    <NumberSetting label={"Update Rate"} step={1} min={256} max={1024}></NumberSetting>
                    <NumberSetting label={"Scene Scale"} step={1} min={256} max={1024}></NumberSetting>
                    <NumberSetting label={"Scene Exposure"} step={1} min={256} max={1024}></NumberSetting>
                    <NumberSetting label={"Scene Indirect Intensity"} step={1} min={256} max={1024}></NumberSetting>
                </div>
                <VideoSettings></VideoSettings>
                <LayerSettings></LayerSettings>
                <MeshSettings></MeshSettings>
                <Form.Select class="mb-4" onChange={(event) => {}}>
                    <option value="line">Line Based</option>
                    <option value="loop">Loop Based</option>
                </Form.Select>
                <Switch>
                    <Match when={props.settings().mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE}>
                        <LineSettings></LineSettings>
                    </Match>
                    <Match when={props.settings().mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP}>
                        <LoopSettings></LoopSettings>
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