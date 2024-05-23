import { mat4, vec3 } from "gl-matrix";

export enum DisplayType
{
    Desktop,
    AR
}

export type OnDisplayRender = () => void;
export type OnDisplayClose = () => void;

export interface Display
{
    create() : Promise<boolean>;
    destroy() : void;
    show() : void;
    calibrate(origin : vec3, side_x : vec3) : void;

    set_on_render(callback : OnDisplayRender) : void;
    set_on_close(callback : OnDisplayClose) : void;

    get_projection_matrix() : mat4;
    get_view_matrix() : mat4;
    get_position() : vec3;
    get_type() : DisplayType;

    requires_calibration() : boolean;
}

export function build_display(type : DisplayType, canvas : HTMLCanvasElement, gl : WebGL2RenderingContext) : Display | null
{
    switch(type)
    {
    case DisplayType.Desktop:
        return new DesktopDisplay(canvas);
    case DisplayType.AR:
        return new ARDisplay(gl);
    default:
        break;
    }

    return null;
}

const DESKTOP_DISPLAY_VIEW_UPDATE_RATE = 16; //In milliseconds
const DESKTOP_DISPLAY_MOVEMENT_SPEED = 1.5;
const DESKTOP_DISPLAY_ROTATION_SPEED = 0.05;
const DESKTOP_DISPLAY_FIELD_OF_VIEW = 80.0;
const DESKTOP_DISPLAY_NEAR_DISTANCE = 0.1;
const DESKTOP_DISPLAY_FAR_DISTANCE = 200.0;

class DesktopDisplay
{
    private canvas : HTMLCanvasElement;
    private frame_request : number | null = null;
    private view_interval : number | null = null;

    private projection_matrix = mat4.create();
    private view_matrix = mat4.create();
    private position = vec3.create();
    private vertical_angle = 0.0;
    private horizontal_angle = -Math.PI;
    
    private input_forward = false;
    private input_backward = false;
    private input_left = false;
    private input_right = false;
    private input_up = false;
    private input_down = false;

    private on_render : OnDisplayRender | null = null;
    private on_close : OnDisplayClose | null = null;

    constructor(canvas: HTMLCanvasElement)
    {
        this.canvas = canvas;
    }

    async create() : Promise<boolean>
    {
        this.canvas.onresize = this.on_resize;
        this.canvas.onmousemove = this.on_mouse_move;
        this.canvas.onkeydown = this.on_key_down;
        this.canvas.onkeyup = this.on_key_up

        vec3.zero(this.position);
        this.compute_projection_matrix();
        this.compute_view_matrix();
        
        this.view_interval = setInterval(this.compute_view_matrix.bind(this), DESKTOP_DISPLAY_VIEW_UPDATE_RATE);

        return true;
    }

    destroy()
    {
        if(this.frame_request != null)
        {
            cancelAnimationFrame(this.frame_request);   
        }

        if(this.view_interval != null)
        {
            clearInterval(this.view_interval);   
        }
    }

    show()
    {
        if(this.frame_request != null)
        {
            return;
        }

        const render_function = () =>
        {
            if(this.on_render != null)
            {
                this.on_render();

                this.frame_request = requestAnimationFrame(render_function); 
            }

            else
            {
                this.frame_request = null;
            }
        };

        this.frame_request = requestAnimationFrame(render_function);
    }

    calibrate(origin : vec3, side_x : vec3)
    {
        
    }

    set_on_render(callback : OnDisplayRender)
    {
        this.on_render = callback;
    }

    set_on_close(callback : OnDisplayClose)
    {
        this.on_close = callback;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_position() : vec3
    {
        return this.position;
    }

    get_type() : DisplayType
    {
        return DisplayType.Desktop;
    }

    requires_calibration() : boolean
    {
        return false;
    }

    private compute_projection_matrix()
    {
        const aspect_ratio = this.canvas.width / this.canvas.height;   
        mat4.perspective(this.projection_matrix, DESKTOP_DISPLAY_FIELD_OF_VIEW, aspect_ratio, DESKTOP_DISPLAY_NEAR_DISTANCE, DESKTOP_DISPLAY_FAR_DISTANCE);
    }

    private compute_view_matrix()
    {
        const forward = vec3.fromValues(0,0,1);
        const side = vec3.fromValues(1,0,0);
        const up = vec3.fromValues(0,1,0);

        const delta_position = vec3.create();
        const delta_time = 1.0 / DESKTOP_DISPLAY_VIEW_UPDATE_RATE;
        
        if(this.input_forward)
        {
            vec3.scaleAndAdd(delta_position, delta_position, forward, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_backward)
        {
            vec3.scaleAndAdd(delta_position, delta_position, forward, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_left)
        {
            vec3.scaleAndAdd(delta_position, delta_position, side, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_right)
        {
            vec3.scaleAndAdd(delta_position, delta_position, side, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_up)
        {
            vec3.scaleAndAdd(delta_position, delta_position, up, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_down)
        {
            vec3.scaleAndAdd(delta_position, delta_position, up, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        vec3.rotateY(delta_position, delta_position, vec3.fromValues(0.0, 0.0, 0.0), this.horizontal_angle);
        vec3.add(this.position, this.position, delta_position);

        let translation = vec3.create();
        vec3.negate(translation, this.position);

        mat4.identity(this.view_matrix);
        mat4.rotateX(this.view_matrix, this.view_matrix, this.vertical_angle);
        mat4.rotateY(this.view_matrix, this.view_matrix, this.horizontal_angle);
        mat4.translate(this.view_matrix, this.view_matrix, translation);
    }

    private on_resize(even : UIEvent)
    {
        this.compute_projection_matrix();
    }

    private on_mouse_move(event : MouseEvent)
    {
        if(event.button == 0)
        {
            this.horizontal_angle += event.movementX * DESKTOP_DISPLAY_ROTATION_SPEED;
            this.vertical_angle += event.movementY * DESKTOP_DISPLAY_ROTATION_SPEED;
            
            if(this.horizontal_angle >= 0.0)
            {
                this.horizontal_angle = this.horizontal_angle % (Math.PI * 2.0);
            }

            else
            {
                this.horizontal_angle = (Math.PI * 2.0) - (this.horizontal_angle % (Math.PI * 2.0));
            }

            this.vertical_angle = Math.min(Math.max(this.vertical_angle, -Math.PI / 2.0), Math.PI / 2.0);
        }
    }

    private on_key_down(event : KeyboardEvent)
    {
        switch(event.key)
        {
        case "KeyW":
            this.input_forward = true;
            break;
        case "KeyA":
            this.input_left = true;
            break;
        case "KeyS":
            this.input_backward = true;
            break;
        case "KeyD":
            this.input_right= true;
            break;
        case "Space":
            this.input_up = true;
            break;
        case "Shift":
            this.input_down = true;
            break;
        default:
            break;
        }
    }

    private on_key_up(event : KeyboardEvent)
    {
        switch(event.key)
        {
        case "KeyW":
            this.input_forward = false;
            break;
        case "KeyA":
            this.input_left = false;
            break;
        case "KeyS":
            this.input_backward = false;
            break;
        case "KeyD":
            this.input_right= false;
            break;
        case "Space":
            this.input_up = false;
            break;
        case "Shift":
            this.input_down = false;
            break;
        default:
            break;
        }
    }
}

const AR_DISPLAY_NEAR_DISTANCE = 0.1;
const AR_DISPLAY_FAR_DISTANCE = 200.0;

class ARDisplay
{
    private gl : WebGL2RenderingContext;
    private session : XRSystem | null = null;
    private reference_space : XRReferenceSpace | null = null;
    private frame_request : number | null = null;

    private projection_matrix = mat4.create();
    private view_matrix = mat4.create();
    private position = vec3.create();

    private on_render : OnDisplayRender | null = null;
    private on_close : OnDisplayClose | null = null;

    constructor(gl : WebGL2RenderingContext)
    {
        this.gl = gl;
    }

    async create() : Promise<boolean>
    {
        if(!("xr" in navigator) || navigator.xr == null)
        {
            return false;
        }

        this.session = await navigator.xr.requestSession("immersive-ar").catch((error : any) =>
        {
            return null;
        });

        if(this.session == null)
        {
            return false;   
        }

        this.reference_space = await this.session.requestReferenceSpace("local").catch((error : any) =>
        {
            return null;
        });

        if(this.reference_space == null)
        {
            return false;   
        }

        const render_state =
        {
            baseLayer: new XRWebGLLayer(this.session, this.gl),
            depthNear: AR_DISPLAY_NEAR_DISTANCE,
            depthFar: AR_DISPLAY_FAR_DISTANCE,
        };

        this.session.updateRenderState(render_state);

        this.session.addEventListener("end", () =>
        {
            if(this.on_close != null)
            {
                this.on_close();
            }
        });

        return true;
    }

    destroy()
    {
        if (this.session != null)
        {
            if(this.frame_request != null)
            {
                this.session.cancelAnimationFrame(this.frame_request);
                this.frame_request = null;
            }

            this.session.end();

            this.reference_space = null;
            this.session = null;
        }
    }

    calibrate(origin : vec3, side_x : vec3)
    {
        //TODO: implement calibration calculation

        return true;
    }

    set_on_render(callback : OnDisplayRender)
    {
        this.on_render = callback;

        const render_function = (time: number, frame: XRFrame) =>
        {
            if(this.on_render == null)
            {
                return;    
            }

            this.compute_projection_matrix(frame);
            this.compute_view_matrix(frame);
            
            if(this.on_view_change != null)
            {
                this.on_view_change();   
            }

            if(this.on_render())
            {
                this.frame_request = this.session.requestAnimationFrame(render_function);
            }

            else
            {
                this.frame_request = null;

                if(this.on_close == null)
                {
                    return;
                }

                this.on_close();
            }
        };

        this.frame_request = this.session.requestAnimationFrame(render_function);
    }

    set_on_close(callback : OnDisplayClose)
    {
        this.on_close = callback;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_position() : vec3
    {
        return this.position;
    }

    get_type() : DisplayType
    {
        return DisplayType.AR;
    }

    requires_calibration() : boolean
    {
        return true;
    }

    private compute_projection_matrix(frame: XRFrame)
    {
        const pose = frame.getViewerPose(this.reference_space);
        const view = pose.views[0];

        mat4.copy(this.projection_matrix, view.projectionMatrix);

        //TODO: check projection_matrix
    }

    private compute_view_matrix(frame: XRFrame)
    {
        const pose = frame.getViewerPose(this.reference_space);
        const view = pose.views[0];

        //TODO: compute view_matrix
        //TODO: compute position
    }
}