import { Component, Show, createSignal } from "solid-js";
import { createStore } from "solid-js/store";
import { render } from "solid-js/web";
import { Button } from "solid-bootstrap";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, DisplayType, SessionMode, load_wrapper_module, MeshGeneratorType, VideoCodecMode, LoopSettings, LineSettings } from "./remote_rendering";
import { SettingDropdown, SettingFile, SettingNumber, SettingNumberType, SettingScene } from "./components/setting";
import { create_local_store } from "./components/local_storage";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    const default_mesh_config = props.wrapper.default_mesh_settings();
    const default_video_config = props.wrapper.default_video_settings();

    const default_config =
    {
        scene_file_name: "",
        scene_scale: 0.01,
        scene_exposure: 1.0,
        scene_indirect_intensity: 1.0,
        sky_file_name: "",
        sky_intensity: 1.0,
        render_resolution: 1024,
        render_rate: 1000,
        layer_depth_base_threshold: default_mesh_config.layer.depth_base_threshold,
        layer_depth_slope_threshold: default_mesh_config.layer.depth_slope_threshold,
        layer_use_object_ids: default_mesh_config.layer.use_object_ids ? "Enabled" : "Disabled",
        mesh_generator: "Loop-Based",
        mesh_depth_max: default_mesh_config.mesh.depth_max,
        line_laplace_threshold: default_mesh_config.mesh.line?.laplace_threshold ?? 0.0,
        line_normal_scale: default_mesh_config.mesh.line?.normal_scale ?? 0.0,
        line_line_length_min: default_mesh_config.mesh.line?.line_length_min ?? 0.0,
        loop_depth_base_threshold: default_mesh_config.mesh.loop?.depth_base_threshold ?? 0.0,
        loop_depth_slope_threshold: default_mesh_config.mesh.loop?.depth_slope_threshold ?? 0.0,
        loop_normal_threshold: default_mesh_config.mesh.loop?.normal_threshold ?? 0.0,
        loop_triangle_scale: default_mesh_config.mesh.loop?.triangle_scale ?? 0.0,
        loop_loop_length_min: default_mesh_config.mesh.loop?.loop_length_min ?? 0.0,
        loop_layer_config: "Dual-Layer",
        loop_use_normals: default_mesh_config.mesh.loop?.use_normals ? "Enabled" : "Disabled",
        loop_use_object_ids: default_mesh_config.mesh.loop?.use_object_ids ? "Enabled" : "Disabled",
        video_mode: "Constant Quality",
        video_framerate: default_video_config.framerate,
        video_bitrate: default_video_config.bitrate,
        video_quality: default_video_config.quality,
        video_use_chroma_subsampling: "Enabled",
        evaluation_mode: "Capture",
        evaluation_animation_file: "",
        evaluation_output_directory: "/",
        evaluation_export_color: "Disabled",
        evaluation_export_depth: "Disabled",
        evaluation_export_mesh: "Disabled",
        evaluation_export_feature_lines: "Disabled"
    };

    const default_config_clone = JSON.parse(JSON.stringify(default_config));

    const [show_settings, set_show_settings] = createSignal(true);
    const [config, set_config] = create_local_store("loop_based_dual_layer_reprojection", default_config_clone);
    
    let session_reset : (() => void) | null = null;

    function session_start()
    {
        if(session_reset != null)
        {
            session_reset();   
        }

        set_show_settings(false);
    }

    function register_session_reset(reset : () => void)
    {
        session_reset = reset;
    }

    function convert_boolean(value : string) : boolean
    {
        switch(value)
        {
        case "Enabled":
            return true;
        case "Disbaled":
            return false;
        default:
            break;
        }

        return false;
    }

    function get_session_config()
    {
        let session_mode = SessionMode.Capture;

        switch(config.evaluation_mode)
        {
        case "Capture":
            session_mode = SessionMode.Capture;
            break;
        case "Benchmark":
            session_mode = SessionMode.Benchmark;
            break;
        case "Replay Method":
            session_mode = SessionMode.ReplayMethod;
            break;
        case "Replay Groundtruth":
            session_mode = SessionMode.ReplayGroundTruth;
            break;
        default:
            break;
        }

        let layer_count = 2;

        switch(config.loop_layer_config)
        {
        case "Single-Layer":
            layer_count = 1;
            break;
        case "Dual-Layer":
            layer_count = 2;
            break;
        default:
            break;
        }

        let mesh_generator : MeshGeneratorType = props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP;

        switch(config.mesh_generator)
        {
        case "Line-Based":
            mesh_generator = props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE;
            layer_count = 1; //The line based method only works with one layer
            break;
        case "Loop-Based":
            mesh_generator = props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP;
            break;
        default:
            break;
        }

        let video_mode : VideoCodecMode= props.wrapper.VideoCodecMode.VIDEO_CODEC_MODE_CONSTANT_BITRATE;

        switch(config.video_mode)
        {
        case "Constant Quality":
            video_mode = props.wrapper.VideoCodecMode.VIDEO_CODEC_MODE_CONSTANT_QUALITY
            break;
        case "Constant Bitrate":
            video_mode = props.wrapper.VideoCodecMode.VIDEO_CODEC_MODE_CONSTANT_BITRATE
            break;
        default:
            break;
        }

        //Only add the parameters to the config if the method is used
        let line_config : LineSettings | undefined = undefined;
        let loop_config : LoopSettings | undefined = undefined;

        if(mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE)
        {
            line_config =
            {
                laplace_threshold: config.line_laplace_threshold,
                normal_scale: config.line_normal_scale,
                line_length_min: config.line_line_length_min
            };
        }

        else if(mesh_generator == props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP)
        {
            loop_config =
            {
                depth_base_threshold: config.loop_depth_base_threshold,
                depth_slope_threshold: config.loop_depth_slope_threshold,
                normal_threshold: config.loop_normal_threshold,
                triangle_scale: config.loop_triangle_scale,
                loop_length_min: config.loop_loop_length_min,
                use_normals: convert_boolean(config.loop_use_normals) ? 1 : 0,
                use_object_ids: convert_boolean(config.loop_use_object_ids) ? 1 : 0
            };
        }

        const session_config : SessionConfig =
        {
            mode: session_mode,
            output_path: config.evaluation_output_directory,
            animation_file_name: config.evaluation_animation_file,
            resolution: config.render_resolution,
            layer_count,
            render_request_rate: config.render_rate,
            mesh_generator,
            mesh_settings:
            {
                layer:
                {
                    depth_base_threshold: config.layer_depth_base_threshold,
                    depth_slope_threshold: config.layer_depth_slope_threshold,
                    use_object_ids: convert_boolean(config.layer_use_object_ids) ? 1 : 0
                },
                mesh:
                {
                    depth_max: config.mesh_depth_max,
                    line: line_config,
                    loop: loop_config
                }
            },
            video_settings:
            {
                mode: video_mode,
                framerate: config.video_framerate,
                bitrate: config.video_bitrate,
                quality: config.video_quality
            },
            video_use_chroma_subsampling: convert_boolean(config.video_use_chroma_subsampling),
            scene_file_name: config.scene_file_name,
            scene_scale: config.scene_scale,
            scene_exposure: config.scene_exposure,
            scene_indirect_intensity: config.scene_indirect_intensity,
            sky_file_name: config.sky_file_name,
            sky_intensity: config.sky_intensity,
            export_color: convert_boolean(config.evaluation_export_color),
            export_depth: convert_boolean(config.evaluation_export_depth),
            export_mesh: convert_boolean(config.evaluation_export_mesh),
            export_feature_lines: convert_boolean(config.evaluation_export_feature_lines)
        };

        return session_config;
    }

    function on_session_config_reset()
    {
        set_config(default_config);
    }

    return (
        <Router>
            <Route path="/" component=
            {() => 
                <div class="container">
                    <Show when={show_settings()}>
                        <div class="d-flex justify-content-center" style="margin-top: 64px; margin-bottom: 96px;">
                            <h1 class="display-4">Loop-Based Dual-Layer Reprojection</h1>
                        </div>
                        <form onsubmit={() => session_start()}>
                            <div>
                                <h4 class="border-bottom my-4">Scene Settings</h4>
                                <SettingScene label="Scene File" value={config.scene_file_name} set_value={value => set_config("scene_file_name", value)}></SettingScene>
                                <SettingNumber label="Scene Scale" value={config.scene_scale} set_value={value => set_config("scene_scale", value)} min_value={0.01} max_value={1.0} type={SettingNumberType.Float} step={0.01}></SettingNumber>
                                <SettingNumber label="Scene Exposure" value={config.scene_exposure} set_value={value => set_config("scene_exposure", value)} min_value={0.0} max_value={2.0} type={SettingNumberType.Float} step={0.1}></SettingNumber>
                                <SettingNumber label="Scene Indirect Intensity" value={config.scene_indirect_intensity} set_value={value => set_config("scene_indirect_intensity", value)} min_value={0.0} max_value={2.0} type={SettingNumberType.Float} step={0.1}></SettingNumber>
                                <SettingFile label="Sky File" select_type="file" value={config.sky_file_name} set_value={value => set_config("sky_file_name", value)}></SettingFile>
                                <SettingNumber label="Sky Intensity" value={config.sky_intensity} set_value={value => set_config("sky_intensity", value)} min_value={0.0} max_value={2.0} type={SettingNumberType.Float} step={0.1}></SettingNumber>
                            </div>
                            <div>
                                <h4 class="border-bottom my-4">Server Settings</h4>
                                <SettingNumber label="Render Resolution" value={config.render_resolution} set_value={value => set_config("render_resolution", value)} min_value={256} max_value={1024} step={256}></SettingNumber>
                                <SettingNumber label="Request Rate" value={config.render_rate} set_value={value => set_config("render_rate", value)} min_value={100} max_value={2000}></SettingNumber>
                            </div>
                            <div>
                                <h4 class="border-bottom my-4">Layer Settings</h4>
                                <SettingNumber label="Depth Base Threshold" value={config.layer_depth_base_threshold} set_value={value => set_config("layer_depth_base_threshold", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                <SettingNumber label="Depth Slope Threshold" value={config.layer_depth_slope_threshold} set_value={value => set_config("layer_depth_slope_threshold", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                <SettingDropdown label="Use Object IDs" value={config.layer_use_object_ids} set_value={value => set_config("layer_use_object_ids", value)}>
                                    <option>Enabled</option>
                                    <option>Disabled</option>
                                </SettingDropdown>
                            </div>
                            <div>
                                <h4 class="border-bottom my-4">Mesh Settings</h4>
                                <SettingDropdown label="Method" value={config.mesh_generator} set_value={value => set_config("mesh_generator", value)}>
                                    <option>Line-Based</option>
                                    <option>Loop-Based</option>
                                </SettingDropdown>
                                <SettingNumber label="Depth Max" value={config.mesh_depth_max} set_value={value => set_config("mesh_depth_max", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                <Show when={config.mesh_generator == "Line-Based"}>
                                    <SettingNumber label="Laplace Threshold" value={config.line_laplace_threshold} set_value={value => set_config("line_laplace_threshold", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                    <SettingNumber label="Normal Scale" value={config.line_normal_scale} set_value={value => set_config("line_normal_scale", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.0001}></SettingNumber>
                                    <SettingNumber label="Line Length Min" value={config.line_line_length_min} set_value={value => set_config("line_line_length_min", value)} min_value={0} max_value={100}></SettingNumber>
                                </Show>
                                <Show when={config.mesh_generator == "Loop-Based"}>
                                    <SettingNumber label="Depth Base Threshold" value={config.loop_depth_base_threshold} set_value={value => set_config("loop_depth_base_threshold", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                    <SettingNumber label="Depth Slope Threshold" value={config.loop_depth_slope_threshold} set_value={value => set_config("loop_depth_slope_threshold", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.001}></SettingNumber>
                                    <SettingNumber label="Normal Threshold" value={config.loop_normal_threshold} set_value={value => set_config("loop_normal_threshold", value)} min_value={0.0} max_value={10.0} type={SettingNumberType.Float} step={0.0001}></SettingNumber>
                                    <SettingNumber label="Triangle Scale" value={config.loop_triangle_scale} set_value={value => set_config("loop_triangle_scale", value)} min_value={0.0} max_value={10.0}></SettingNumber>
                                    <SettingNumber label="Loop Length Min" value={config.loop_loop_length_min} set_value={value => set_config("loop_loop_length_min", value)} min_value={0.0} max_value={500.0}></SettingNumber>
                                    <SettingDropdown label="Layer Config" value={config.loop_layer_config} set_value={value => set_config("loop_layer_config", value)}>
                                        <option>Single-Layer</option>
                                        <option>Dual-Layer</option>
                                    </SettingDropdown>
                                    <SettingDropdown label="Use Normals" value={config.loop_use_normals} set_value={value => set_config("loop_use_normals", value)}>
                                        <option>Enabled</option>
                                        <option>Disabled</option>
                                    </SettingDropdown>
                                    <SettingDropdown label="Use Object IDs" value={config.loop_use_object_ids} set_value={value => set_config("loop_use_object_ids", value)}>
                                        <option>Enabled</option>
                                        <option>Disabled</option>
                                    </SettingDropdown>
                                </Show>
                            </div>
                            <div>
                                <h4 class="border-bottom my-4">Video Settings</h4>
                                <SettingDropdown label="Mode" value={config.video_mode} set_value={value => set_config("video_mode", value)}>
                                    <option>Constant Bitrate</option>
                                    <option>Constant Quality</option>
                                </SettingDropdown>
                                <SettingNumber label="Framerate" value={config.video_framerate} set_value={value => set_config("video_framerate", value)} min_value={1} max_value={120}></SettingNumber>
                                <Show when={config.video_mode == "Constant Bitrate"}>
                                    <SettingNumber label="Bitrate" value={config.video_bitrate} set_value={value => set_config("video_bitrate", value)} min_value={1.0} max_value={200.0} step={1.0}></SettingNumber>
                                </Show>
                                <Show when={config.video_mode == "Constant Quality"}>
                                    <SettingNumber label="Quality" value={config.video_quality} set_value={value => set_config("video_quality", value)} min_value={0.0} max_value={1.0} type={SettingNumberType.Float} step={0.01}></SettingNumber>
                                </Show>
                                <SettingDropdown label="Chroma Subsampling" value={config.video_use_chroma_subsampling} set_value={value => set_config("video_use_chroma_subsampling", value)}>
                                        <option>Enabled</option>
                                        <option>Disabled</option>
                                </SettingDropdown>
                            </div>
                            <div>
                                <h4 class="border-bottom my-4">Evaluation</h4>
                                <SettingDropdown label="Mode" value={config.evaluation_mode} set_value={value => set_config("evaluation_mode", value)}>
                                    <option>Capture</option>
                                    <option>Benchmark</option>
                                    <option>Replay Method</option>
                                    <option>Replay Groundtruth</option>
                                </SettingDropdown>
                                <Show when={config.evaluation_mode != "Capture"}>
                                    <SettingFile label="Animation File" select_type="file" value={config.evaluation_animation_file} set_value={value => set_config("evaluation_animation_file", value)}></SettingFile>
                                </Show>
                                <Show when={config.evaluation_mode == "Replay Method"}>
                                    <SettingDropdown label="Export Color" value={config.evaluation_export_color} set_value={value => set_config("evaluation_export_color", value)}>
                                            <option>Enabled</option>
                                            <option>Disabled</option>
                                    </SettingDropdown>
                                    <SettingDropdown label="Export Depth" value={config.evaluation_export_depth} set_value={value => set_config("evaluation_export_depth", value)}>
                                            <option>Enabled</option>
                                            <option>Disabled</option>
                                    </SettingDropdown>
                                    <SettingDropdown label="Export Mesh" value={config.evaluation_export_mesh} set_value={value => set_config("evaluation_export_mesh", value)}>
                                            <option>Enabled</option>
                                            <option>Disabled</option>
                                    </SettingDropdown>
                                    <SettingDropdown label="Export Feature Lines" value={config.evaluation_export_feature_lines} set_value={value => set_config("evaluation_export_feature_lines", value)}>
                                            <option>Enabled</option>
                                            <option>Disabled</option>
                                    </SettingDropdown>
                                </Show>
                                <SettingFile label="Output Directory" select_type="directory" value={config.evaluation_output_directory} set_value={value => set_config("evaluation_output_directory", value)}></SettingFile>
                            </div>
                            <div class="d-flex justify-content-between py-5 mx-3">
                                <div class="bg-light rounded p-3">
                                    <Button onClick={on_session_config_reset}>Reset</Button>
                                </div>
                                <div class="bg-light rounded p-3">
                                    <Button type="submit">Confirm</Button>
                                </div>
                            </div>
                        </form>
                    </Show>
                    <Show when={!show_settings()}>
                        <RemoteRendering wrapper={props.wrapper} config={get_session_config()} register_reset={register_session_reset} on_close={() => set_show_settings(true)} preferred_display={DisplayType.AR}></RemoteRendering>
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