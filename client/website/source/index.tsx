import { Component } from "solid-js";
import { render } from "solid-js/web";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, SessionMode, load_wrapper_module } from "./remote_rendering";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    glMatrix.setMatrixArrayType(Array);

    let config : SessionConfig =
    {
        mode: SessionMode.Capture,
        output_path: "",
        resolution_width: 0,
        resolution_height: 0,
        layer_count: 0,
        render_request_rate: 100,
        mesh_generator: undefined,
        mesh_settings: {
            layer: {
                depth_base_threshold: 0,
                depth_slope_threshold: 0,
                use_object_ids: 0
            },
            mesh: {
                depth_max: 0,
                line: undefined,
                loop: undefined
            }
        },
        video_codec: undefined,
        video_settings: {
            mode: undefined,
            framerate: 0,
            bitrate: 0,
            quality: 0
        },
        video_use_chroma_subsampling: 0,
        scene_file_name: "",
        scene_scale: 0,
        scene_exposure: 0,
        scene_indirect_intensity: 0,
        sky_file_name: "",
        sky_intensity: 0
    };

    function test()
    {

    }

    return (
        <Router>
            <Route path="/" component={() => <RemoteRendering wrapper={props.wrapper} config={config} on_close={test}></RemoteRendering>} />
        </Router>
    );
};

load_wrapper_module().then(wrapper =>
{
    const app = document.getElementById("app");

    render(() => App({wrapper}), app!); 
});