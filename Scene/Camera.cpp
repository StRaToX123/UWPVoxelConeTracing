#include <Scene/Camera.h>



Camera::Camera()
    : is_dirty_view_matrix( true )
    , is_dirty_projection_matrix( true )
    , vertical_fov_degrees( 45.0f )
    , aspect_ratio( 1.0f )
    , z_near( 0.1f )
    , z_far( 100.0f )
{
    p_data = (AlignedData*)_aligned_malloc( sizeof(AlignedData), 16 );
    p_data->translation = XMVectorZero();
    p_data->rotation_quaternion = XMQuaternionIdentity();
}

Camera::~Camera()
{
    _aligned_free(p_data);
}

void XM_CALLCONV Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up)
{
    p_data->view_matrix = XMMatrixLookAtLH( eye, target, up );
    p_data->translation = eye;
    p_data->rotation_quaternion = XMQuaternionRotationMatrix( XMMatrixTranspose(p_data->view_matrix) );

    is_dirty_view_matrix = false;
}

XMMATRIX Camera::GetViewMatrix()
{
    if ( is_dirty_view_matrix )
    {
        UpdateViewMatrix();
    }
    return p_data->view_matrix;
}


void Camera::SetProjection(float fovy, float aspect, float zNear, float zFar)
{
    vertical_fov_degrees = fovy;
    aspect_ratio = aspect;
    z_near = zNear;
    z_far = zFar;
    is_dirty_projection_matrix = true;
}

XMMATRIX Camera::GetProjectionMatrix()
{
    if (is_dirty_projection_matrix)
    {
        UpdateProjectionMatrix();
    }

    return p_data->projection_matrix;
}


void Camera::SetFoV(float fovyInDegrees)
{
    if (vertical_fov_degrees != fovyInDegrees)
    {
        vertical_fov_degrees = fovyInDegrees;
        is_dirty_projection_matrix = true;
    }
}

float Camera::GetFoV() const
{
    return vertical_fov_degrees;
}


void XM_CALLCONV Camera::SetTranslation(FXMVECTOR translation)
{
    p_data->translation = translation;
    is_dirty_view_matrix = true;
}

XMVECTOR Camera::GetTranslation() const
{
    return p_data->translation;
}

void Camera::SetRotation(FXMVECTOR quaternion)
{
    p_data->rotation_quaternion = quaternion;
    is_dirty_view_matrix = true;
}

XMVECTOR Camera::GetRotationQuaternion() const
{
    return p_data->rotation_quaternion;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space)
{
    switch ( space )
    {
    case Space::Local:
        {
            p_data->translation += XMVector3Rotate( translation, p_data->rotation_quaternion );
        }
        break;
    case Space::World:
        {
            p_data->translation += translation;
        }
        break;
    }

    p_data->translation = XMVectorSetW( p_data->translation, 1.0f );

    is_dirty_view_matrix = true;
}

void Camera::Rotate(FXMVECTOR quaternion)
{
    p_data->rotation_quaternion = XMQuaternionMultiply(p_data->rotation_quaternion, quaternion);
    is_dirty_view_matrix = true;
}

void Camera::UpdateViewMatrix()
{
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(p_data->rotation_quaternion));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector(-(p_data->translation));

    p_data->view_matrix = translationMatrix * rotationMatrix;
    is_dirty_view_matrix = false;
}

void Camera::UpdateProjectionMatrix()
{
    p_data->projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(vertical_fov_degrees), aspect_ratio, z_near, z_far);
    is_dirty_projection_matrix = false;
}
