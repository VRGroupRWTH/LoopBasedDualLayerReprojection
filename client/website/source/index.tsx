import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Button } from "solid-bootstrap";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, DisplayType, SessionMode, load_wrapper_module } from "./remote_rendering";
import { SettingDropdown, SettingFile, SettingNumber, SettingScene } from "./components/settings";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    const default_config  : SessionConfig =
    {
        mode: SessionMode.Capture,
        output_path: "",
        animation_file_name: "",
        resolution_width: 1024,
        resolution_height: 1024,
        layer_count: 2,
        render_request_rate: 1000,
        mesh_generator: props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP,
        mesh_settings: props.wrapper.default_mesh_settings(),
        video_settings: props.wrapper.default_video_settings(),
        video_use_chroma_subsampling: true,
        scene_file_name: "",
        scene_scale: 0.01,
        scene_exposure: 1.0,
        scene_indirect_intensity: 1.0,
        sky_file_name: "",
        sky_intensity: 1.0
    };

    const [show_settings, set_show_settings] = createSignal(true);
    const [config, set_config] = createSignal(default_config);

    return (
        <Router>
            <Route path="/" component=
            {() => 
                <div class="container">
                    <Show when={show_settings()}>
                        <div class="d-flex justify-content-center" style="margin-top: 64px; margin-bottom: 96px;">
                            <h1 class="display-4">Loop-Based Dual-Layer Reprojection</h1>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Scene Settings</h4>
                            <SettingScene label="Scene File"></SettingScene>
                            <SettingNumber label="Scene Scale"></SettingNumber>
                            <SettingNumber label="Scene Exposure"></SettingNumber>
                            <SettingNumber label="Scene Indirect Intensity"></SettingNumber>
                            <SettingFile label="Sky File" select_type="file"></SettingFile>
                            <SettingNumber label="Sky Intensity"></SettingNumber>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Server Settings</h4>
                            <SettingNumber label="Render Resolution"></SettingNumber>
                            <SettingNumber label="Request Rate"></SettingNumber>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Layer Settings</h4>
                            <SettingNumber label="Depth Base Threshold"></SettingNumber>
                            <SettingNumber label="Depth Slope Threshold"></SettingNumber>
                            <SettingDropdown label="Use Object IDs">
                                <option>Enabled</option>
                                <option>Disabled</option>
                            </SettingDropdown>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Mesh Settings</h4>
                            <SettingDropdown label="Method">
                                <option>Line-Based</option>
                                <option>Loop-Based</option>
                            </SettingDropdown>
                            <SettingNumber label="Depth Max"></SettingNumber>
                            <Show when={config().mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE}>
                                <SettingNumber label="Laplace Threshold"></SettingNumber>
                                <SettingNumber label="Normal Scale"></SettingNumber>
                                <SettingNumber label="Line Length Min"></SettingNumber>
                            </Show>
                            <Show when={config().mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP}>
                                <SettingNumber label="Depth Base Threshold"></SettingNumber>
                                <SettingNumber label="Depth Slope Threshold"></SettingNumber>
                                <SettingNumber label="Normal Threshold"></SettingNumber>
                                <SettingDropdown label="Layer Config">
                                    <option>Single Layer</option>
                                    <option>Dual Layer</option>
                                </SettingDropdown>
                                <SettingDropdown label="Use Normals">
                                    <option>Enabled</option>
                                    <option>Disabled</option>
                                </SettingDropdown>
                                <SettingDropdown label="Use Object IDs">
                                    <option>Enabled</option>
                                    <option>Disabled</option>
                                </SettingDropdown>
                            </Show>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Video Settings</h4>
                            <SettingDropdown label="Mode">
                                <option>Constant Quality</option>
                                <option>Constant Bitrate</option>
                            </SettingDropdown>
                            <SettingNumber label="Framerate"></SettingNumber>
                            <SettingNumber label="Bitrate"></SettingNumber>
                            <SettingNumber label="Quality"></SettingNumber>
                            <SettingDropdown label="Chroma Subsampling">
                                    <option>Enabled</option>
                                    <option>Disabled</option>
                            </SettingDropdown>
                        </div>
                        <div>
                            <h4 class="border-bottom my-4">Evaluation</h4>
                            <SettingDropdown label="Mode">
                                <option>Capture</option>
                                <option>Benchmark</option>
                                <option>Replay Method</option>
                                <option>Replay Groundtruth</option>
                            </SettingDropdown>
                            <SettingFile label="Animation File" select_type="file"></SettingFile>
                            <SettingFile label="Output Folder" select_type="directory"></SettingFile>
                        </div>
                        <div class="d-flex justify-content-end my-5 mx-3">
                            <div class="bg-light rounded p-3">
                                <Button onClick={() => set_show_settings(false)}>Confirm</Button>
                            </div>
                        </div>
                    </Show>
                    <Show when={!show_settings()}>
                        <RemoteRendering wrapper={props.wrapper} config={config()} on_close={() => set_show_settings(true)} preferred_display={DisplayType.AR}></RemoteRendering>
                    </Show>
                </div>
            }/>
        </Router>
    );
};

glMatrix.setMatrixArrayType(Array);

load_wrapper_module().then(wrapper =>
{
    const app = document.getElementById("app");

    render(() => App({wrapper}), app!); 
});