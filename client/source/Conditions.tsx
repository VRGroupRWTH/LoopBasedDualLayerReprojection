import { quat, vec3 } from "gl-matrix";

export interface Technique {
	name: string;
}

export const CONDITION_INTERVALS = [0.5, 1.0, 2.0];
export const CONDITION_TECHNIQUES: Technique[] = [
	{
		name: 'ming',
	},
	{
		name: 'single-layer',
	},
	{
		name: 'dual-layer',
	},
]
export const CONDITION_RESOLUTION = 1024;
export const CONDITION_NEAR_PLANE = 0.01;
export const CONDITION_FAR_PLANE = 1000.0;
export const CONDITION_VIDEO_CODEC = "h264";
export const CONDITION_CHROMA_SUBSAMPLING = false;
export const CONDITION_FILE_NAME = "C:/Users/Simon/Code/scenes/sponza/glTF/Sponza.gltf";

export const TIME_BETWEEN_RUNS = 2000;

export interface AnimationFrame {
	time: number;
	position: vec3
	orientation: quat, 
}

export const ANIMATION: AnimationFrame[] = [
	{ time: 0, position: [0, 1, 0], orientation: [0, 0, 0, 1]},
	{ time: 1, position: [0.1, 1, 0], orientation: [0, 0, 0, 1]},
	{ time: 2, position: [0.1, 1, 0.1], orientation: [0, 0, 0, 1]},
	{ time: 3, position: [0, 1, 0.1], orientation: [0, 0, 0, 1]},
	{ time: 4, position: [0, 1, 0], orientation: [0, 0, 0, 1]},
];

export const REPITIONS = 10;