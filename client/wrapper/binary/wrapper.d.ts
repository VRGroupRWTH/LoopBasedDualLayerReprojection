// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
interface WasmModule {
}

export interface MeshGeneratorTypeValue<T extends number> {
  value: T;
}
export type MeshGeneratorType = MeshGeneratorTypeValue<0>|MeshGeneratorTypeValue<1>;

export interface VideoCodecTypeValue<T extends number> {
  value: T;
}
export type VideoCodecType = VideoCodecTypeValue<0>|VideoCodecTypeValue<1>|VideoCodecTypeValue<2>;

export interface VideoCodecModeValue<T extends number> {
  value: T;
}
export type VideoCodecMode = VideoCodecModeValue<0>|VideoCodecModeValue<1>;

export interface ExportTypesValue<T extends number> {
  value: T;
}
export type ExportTypes = ExportTypesValue<1>|ExportTypesValue<2>|ExportTypesValue<3>|ExportTypesValue<4>;

export type SessionDestroyForm = {

};

export type CountArray = [ number, number, number, number, number, number ];

export type Matrix = [ number, number, number, number, number, number, number, number, number, number, number, number, number, number, number, number ];

export type MatrixArray = [ Matrix, Matrix, Matrix, Matrix, Matrix, Matrix ];

export type Vertex = {
  x: number,
  y: number,
  z: number
};

export type LayerSettings = {
  depth_base_threshold: number,
  depth_slope_threshold: number,
  use_object_ids: number
};

export type LineSettings = {
  laplace_threshold: number,
  normal_scale: number,
  line_length_min: number
};

export type LoopSettings = {
  depth_base_threshold: number,
  depth_slope_threshold: number,
  normal_threshold: number,
  triangle_scale: number,
  use_normals: number,
  use_object_ids: number
};

export type MeshSettings = {
  depth_max: number,
  line: LineSettings | undefined,
  loop: LoopSettings | undefined
};

export type MeshSettingsForm = {
  layer: LayerSettings,
  mesh: MeshSettings
};

export type LineViewMetadata = {
  time_edge_detection: number,
  time_quad_tree: number,
  time_line_trace: number,
  time_triangulation: number
};

export type LoopViewMetadata = {
  time_vector: number,
  time_base: number,
  time_combine: number,
  time_distribute: number,
  time_write: number,
  time_loop_points: number,
  time_triangulation: number,
  time_loop_info: number,
  time_loop_sort: number,
  time_sweep_line: number,
  time_adjacent_two: number,
  time_adjacent_one: number,
  time_interval_update: number,
  time_inside_outside: number,
  time_contour: number,
  loop_count: number,
  segment_count: number,
  point_count: number
};

export type ViewMetadata = {
  time_layer: number,
  time_image_encode: number,
  time_geometry_encode: number,
  line: LineViewMetadata | undefined,
  loop: LoopViewMetadata | undefined
};

export type ViewMetadataArray = [ ViewMetadata, ViewMetadata, ViewMetadata, ViewMetadata, ViewMetadata, ViewMetadata ];

export type LayerResponseForm = {
  request_id: number,
  layer_index: number,
  geometry_bytes: number,
  image_bytes: number,
  view_metadata: ViewMetadataArray,
  view_matrices: MatrixArray,
  vertex_counts: CountArray,
  index_counts: CountArray
};

export type VideoSettingsForm = {
  mode: VideoCodecMode,
  framerate: number,
  bitrate: number,
  quality: number
};

export type FileNameArray = [ ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string, ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string, ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string, ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string ];

export type RenderRequestForm = {
  request_id: number,
  export_file_names: FileNameArray,
  view_matrices: MatrixArray
};

export type SessionCreateForm = {
  mesh_generator: MeshGeneratorType,
  video_codec: VideoCodecType,
  video_use_chroma_subsampling: number,
  projection_matrix: Matrix,
  resolution_width: number,
  resolution_height: number,
  layer_count: number,
  view_count: number,
  scene_file_name: ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string,
  scene_scale: number,
  scene_exposure: number,
  scene_indirect_intensity: number,
  sky_file_name: ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string,
  sky_intensity: number,
  export_enabled: boolean
};

interface EmbindModule {
  MeshGeneratorType: {MESH_GENERATOR_TYPE_LINE: MeshGeneratorTypeValue<0>, MESH_GENERATOR_TYPE_LOOP: MeshGeneratorTypeValue<1>};
  VideoCodecType: {VIDEO_CODEC_TYPE_H264: VideoCodecTypeValue<0>, VIDEO_CODEC_TYPE_H265: VideoCodecTypeValue<1>, VIDEO_CODEC_TYPE_AV1: VideoCodecTypeValue<2>};
  VideoCodecMode: {VIDEO_CODEC_MODE_CONSTANT_BITRATE: VideoCodecModeValue<0>, VIDEO_CODEC_MODE_CONSTANT_QUALITY: VideoCodecModeValue<1>};
  ExportTypes: {EXPORT_TYPE_COLOR: ExportTypesValue<1>, EXPORT_TYPE_DEPTH: ExportTypesValue<2>, EXPORT_TYPE_MESH: ExportTypesValue<3>, EXPORT_TYPE_FEATURE_LINES: ExportTypesValue<4>};
  default_mesh_settings(): MeshSettingsForm;
  default_video_settings(): VideoSettingsForm;
  build_session_create_packet(form: SessionCreateForm): any;
  build_session_destroy_packet(form: SessionDestroyForm): any;
  build_render_request_packet(form: RenderRequestForm): any;
  build_mesh_settings_packet(form: MeshSettingsForm): any;
  build_video_settings_packet(form: VideoSettingsForm): any;
  parse_layer_response_packet(data: any): LayerResponseForm;
  decode_geoemtry(data: any): void;
}
export type MainModule = WasmModule & EmbindModule;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
