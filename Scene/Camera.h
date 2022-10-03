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
        virtual ~Camera();

        void XM_CALLCONV set_LookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up );
        XMMATRIX get_ViewMatrix() const;
        XMMATRIX get_InverseViewMatrix() const;

        /*
         * Set the camera to a perspective projection matrix.
         * @param fovy The vertical field of view in degrees.
         * @param aspect The aspect ratio of the screen.
         * @param zNear The distance to the near clipping plane.
         * @param zFar The distance to the far clipping plane.
         */
        void set_Projection( float fovy, float aspect, float zNear, float zFar );
        XMMATRIX get_ProjectionMatrix() const;
        XMMATRIX get_InverseProjectionMatrix() const;

        /**
         * Set the field of view in degrees.
         */
        void set_FoV(float fovy);

        /**
         * Get the field of view in degrees.
         */
        float get_FoV() const;

        /**
         * Set the camera's position in world-space.
         */
        void XM_CALLCONV set_Translation(FXMVECTOR translation );
        XMVECTOR get_Translation() const;

        /**
         * Set the camera's rotation in world-space.
         * @param rotation The rotation quaternion.
         */
        void XM_CALLCONV set_Rotation(FXMVECTOR rotation );
        /**
         * Query the camera's rotation.
         * @returns The camera's rotation quaternion.
         */
        XMVECTOR get_Rotation() const;

        void XM_CALLCONV Translate(FXMVECTOR translation, Space space = Space::Local );
        void Rotate(FXMVECTOR quaternion );

    protected:
        virtual void UpdateViewMatrix() const;
        virtual void UpdateInverseViewMatrix() const;
        virtual void UpdateProjectionMatrix() const;
        virtual void UpdateInverseProjectionMatrix() const;

        // This data must be aligned otherwise the SSE intrinsics fail
        // and throw exceptions.
        __declspec(align(16)) struct AlignedData
        {
            // World-space position of the camera.
            XMVECTOR m_Translation;
            // World-space rotation of the camera.
            // THIS IS A QUATERNION!!!!
            XMVECTOR m_Rotation;

            XMMATRIX m_ViewMatrix, m_InverseViewMatrix;
            XMMATRIX m_ProjectionMatrix, m_InverseProjectionMatrix;
        };
        AlignedData* pData;

        // projection parameters
        float m_vFoV;   // Vertical field of view.
        float m_AspectRatio; // Aspect ratio
        float m_zNear;      // Near clip distance
        float m_zFar;       // Far clip distance.

        // True if the view matrix needs to be updated.
        mutable bool m_ViewDirty, m_InverseViewDirty;
        // True if the projection matrix needs to be updated.
        mutable bool m_ProjectionDirty, m_InverseProjectionDirty;

    private:

};