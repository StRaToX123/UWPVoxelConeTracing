#pragma once



#include <DirectXMath.h>




using namespace DirectX;

// When performing transformations on the camera, 
// it is sometimes useful to express which space this 
// transformation should be applied.
enum class Space
{
    Local,
    World,
};

class Camera
{
    public:

        Camera();
        ~Camera();

        void XM_CALLCONV SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up );
        XMMATRIX GetViewMatrix();
        /*
        Set the camera to a perspective projection matrix.
        @param fovy The vertical field of view in degrees.
        @param aspect The aspect ratio of the screen.
        @param zNear The distance to the near clipping plane.
        @param zFar The distance to the far clipping plane.
        */
        void SetProjection(float fovy, float aspect, float zNear, float zFar);
        XMMATRIX GetProjectionMatrix();
        // Set the field of view in degrees.
        void SetFoV(float fovyInDegrees);
        //Get the field of view in degrees.
        float GetFoV() const;
        //Set the camera's position in world-space.
        void XM_CALLCONV SetTranslation(FXMVECTOR translation);
        XMVECTOR GetTranslation() const;
        /*
        Set the camera's rotation in world-space.
        @param rotation The rotation quaternion.
        */
        void XM_CALLCONV SetRotation(FXMVECTOR quaternion);
        /*
        Query the camera's rotation.
        @returns The camera's rotation quaternion.
        */
        XMVECTOR GetRotationQuaternion() const;
        void XM_CALLCONV Translate(FXMVECTOR translation, Space space = Space::Local );
        void Rotate(FXMVECTOR quaternion );

    private:
        // This data must be aligned otherwise the SSE intrinsics fail
        // and throw exceptions.
        __declspec(align(16)) struct AlignedData
        {
            // World-space position of the camera.
            XMVECTOR translation;
            // World-space rotation of the camera.
            XMVECTOR rotation_quaternion;
            XMMATRIX view_matrix;
            XMMATRIX projection_matrix;
        };
        AlignedData* p_data;

        void UpdateViewMatrix();
        void UpdateProjectionMatrix();

        // projection parameters
        float vertical_fov_degrees;   // Vertical field of view.
        float aspect_ratio; // Aspect ratio
        float z_near;      // Near clip distance
        float z_far;       // Far clip distance.
        // True if the view matrix needs to be updated.
        bool is_dirty_view_matrix;
        // True if the projection matrix needs to be updated.
        bool is_dirty_projection_matrix;


};