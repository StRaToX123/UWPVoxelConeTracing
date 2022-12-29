#include "Scene/Mesh.h"



const D3D12_INPUT_ELEMENT_DESC ShaderStructureCPUVertexPositionNormalTextureColor::input_elements[] =
{
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};


Mesh::Mesh()
{

}

Mesh::Mesh(bool isStatic)
{
    is_static = isStatic;
    initialized_as_static = false;
    if (isStatic == true)
    {
        is_static = false;
        initialized_as_static = true;
    }

    world_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    local_rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    fence_value_signaling_vertex_and_index_bufferr_residency = UINT64_MAX;
    for (int i = 0; i < c_frame_count; i++)
    {
        per_frame_transform_matrix_buffer_indexes_assigned[i] = false;
        per_frame_transform_matrix_buffer_indexes[i][0] = 0;
        per_frame_transform_matrix_buffer_indexes[i][1] = 0;
    }

    current_frame_index_containing_most_updated_transform_matrix = c_frame_count - 1;
    vertex_and_index_buffer_upload_started = false;
}




void Mesh::InitializeAsSphere(float diameter, size_t tessellation)
{
    if (tessellation < 3)
    {
        throw out_of_range("Mesh::InitializeAsSphere tessellation parameter out of range");
    }

    float radius = diameter / 2.0f;
    size_t verticalSegments = tessellation;
    size_t horizontalSegments = tessellation * 2;
    vertices.reserve((verticalSegments + 1) * (horizontalSegments + 1));
    indices.reserve(verticalSegments * (horizontalSegments + 1) * 6);
    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        float v = 1 - (float)i / verticalSegments;

        float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
        float dy, dxz;

        XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            float u = (float)j / horizontalSegments;

            float longitude = j * XM_2PI / horizontalSegments;
            float dx, dz;

            XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            XMVECTOR normal = XMVectorSet(dx, dy, dz, 0);
            XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

            vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(normal * radius, normal, textureCoordinate));
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    size_t stride = horizontalSegments + 1;
    for (size_t i = 0; i < verticalSegments; i++)
    {
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            size_t nextI = i + 1;
            size_t nextJ = (j + 1) % stride;

            indices.emplace_back(static_cast<uint16_t>( i * stride + j ));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + j));
            indices.emplace_back(static_cast<uint16_t>(i * stride + nextJ));

            indices.emplace_back(static_cast<uint16_t>(i * stride + nextJ));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + j));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + nextJ));
        }
    }

    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
}

void Mesh::InitializeAsCube(float size)
{
    // A cube has six faces, each one pointing in a different direction.
    const int FaceCount = 6;

    static const XMVECTORF32 faceNormals[FaceCount] =
    {
        { 0,  0,  1 },
        { 0,  0, -1 },
        { 1,  0,  0 },
        { -1,  0,  0 },
        { 0,  1,  0 },
        { 0, -1,  0 },
    };

    static const XMVECTORF32 textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    vertices.reserve(6 * 4);
    indices.reserve(6 * 6);

    size /= 2;

    // Create each face in turn.
    for (int i = 0; i < FaceCount; i++)
    {
        XMVECTOR normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

        XMVECTOR side1 = XMVector3Cross(normal, basis);
        XMVECTOR side2 = XMVector3Cross(normal, side1);

        // Six indices (two triangles) per face.
        size_t vbase = vertices.size();
        indices.emplace_back(static_cast<uint16_t>(vbase + 0));
        indices.emplace_back(static_cast<uint16_t>(vbase + 1));
        indices.emplace_back(static_cast<uint16_t>(vbase + 2));

        indices.emplace_back(static_cast<uint16_t>(vbase + 0));
        indices.emplace_back(static_cast<uint16_t>(vbase + 2));
        indices.emplace_back(static_cast<uint16_t>(vbase + 3));

        // Four vertices per face.
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor((normal - side1 - side2) * size, normal, textureCoordinates[0]));
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor((normal - side1 + side2) * size, normal, textureCoordinates[1]));
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor((normal + side1 + side2) * size, normal, textureCoordinates[2]));
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor((normal + side1 - side2) * size, normal, textureCoordinates[3]));
    }

    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
}


inline XMVECTOR Mesh::GetCircleVector(size_t i, size_t tessellation)
{
    float angle = i * XM_2PI / tessellation;
    float dx, dz;

    XMScalarSinCos(&dx, &dz, angle);

    XMVECTORF32 v = { dx, 0, dz, 0 };
    return v;
}

inline XMVECTOR Mesh::GetCircleTangent(size_t i, size_t tessellation)
{
    float angle = (i * XM_2PI / tessellation) + XM_PIDIV2;
    float dx, dz;

    XMScalarSinCos(&dx, &dz, angle);

    XMVECTORF32 v = { dx, 0, dz, 0 };
    return v;
}


void Mesh::CreateCylinderCap(vector<ShaderStructureCPUVertexPositionNormalTextureColor>& vertices, vector<uint16_t>& indices, size_t tessellation, float height, float radius, bool isTop)
{
    // Create cap indices.
    for (size_t i = 0; i < tessellation - 2; i++)
    {
        size_t i1 = (i + 1) % tessellation;
        size_t i2 = (i + 2) % tessellation;

        if (isTop)
        {
            std::swap(i1, i2);
        }

        size_t vbase = vertices.size();
        indices.emplace_back(static_cast<uint16_t>(vbase));
        indices.emplace_back(static_cast<uint16_t>(vbase + i1));
        indices.emplace_back(static_cast<uint16_t>(vbase + i2));
    }

    // Which end of the cylinder is this?
    XMVECTOR normal = g_XMIdentityR1;
    XMVECTOR textureScale = g_XMNegativeOneHalf;

    if (!isTop)
    {
        normal = -normal;
        textureScale *= g_XMNegateX;
    }

    // Create cap vertices.
    for (size_t i = 0; i < tessellation; i++)
    {
        XMVECTOR circleVector = GetCircleVector(i, tessellation);

        XMVECTOR position = (circleVector * radius) + (normal * height);

        XMVECTOR textureCoordinate = XMVectorMultiplyAdd(XMVectorSwizzle<0, 2, 3, 3>(circleVector), textureScale, g_XMOneHalf);

        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(position, normal, textureCoordinate));
    }
}

void Mesh::InitializeAsCone(float diameter, float height, size_t tessellation)
{
    if (tessellation < 3)
    {
        throw std::out_of_range("Mesh::InitializeAsCone tessellation parameter out of range");
    }

    height /= 2;

    XMVECTOR topOffset = g_XMIdentityR1 * height;

    float radius = diameter / 2;
    size_t stride = tessellation + 1;

    vertices.reserve(((tessellation + 1) * 2) + tessellation);
    indices.reserve(((tessellation + 1) * 3) + ((tessellation - 2) * 3));

    // Create a ring of triangles around the outside of the cone.
    for (size_t i = 0; i <= tessellation; i++)
    {
        XMVECTOR circlevec = GetCircleVector(i, tessellation);

        XMVECTOR sideOffset = circlevec * radius;

        float u = (float)i / tessellation;

        XMVECTOR textureCoordinate = XMLoadFloat(&u);

        XMVECTOR pt = sideOffset - topOffset;

        XMVECTOR normal = XMVector3Cross(GetCircleTangent(i, tessellation), topOffset - pt);
        normal = XMVector3Normalize(normal);

        // Duplicate the top vertex for distinct normals
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(topOffset, normal, g_XMZero));
        vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(pt, normal, textureCoordinate + g_XMIdentityR1));

        indices.emplace_back(static_cast<uint16_t>(i * 2));
        indices.emplace_back(static_cast<uint16_t>((i * 2 + 3) % (stride * 2)));
        indices.emplace_back(static_cast<uint16_t>((i * 2 + 1) % (stride * 2)));
    }

    // Create flat triangle fan caps to seal the bottom.
    CreateCylinderCap(vertices, indices, tessellation, height, radius, false);
    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
}

void Mesh::InitializeAsTorus(float diameter, float thickness, size_t tessellation)
{
    if (tessellation < 3)
    {
        throw std::out_of_range("Mesh::InitializeASTorus tessellation parameter out of range");
    }

    size_t stride = tessellation + 1;
    vertices.reserve(stride * 2);
    indices.reserve((stride * 2) * 6);

    // First we loop around the main ring of the torus.
    for (size_t i = 0; i <= tessellation; i++)
    {
        float u = (float)i / tessellation;

        float outerAngle = i * XM_2PI / tessellation - XM_PIDIV2;

        // Create a transform matrix that will align geometry to
        // slice perpendicularly though the current ring position.
        XMMATRIX transform = XMMatrixTranslation(diameter / 2, 0, 0) * XMMatrixRotationY(outerAngle);

        // Now we loop along the other axis, around the side of the tube.
        for (size_t j = 0; j <= tessellation; j++)
        {
            float v = 1 - (float)j / tessellation;

            float innerAngle = j * XM_2PI / tessellation + XM_PI;
            float dx, dy;

            XMScalarSinCos(&dy, &dx, innerAngle);

            // Create a vertex.
            XMVECTOR normal = XMVectorSet(dx, dy, 0, 0);
            XMVECTOR position = normal * thickness / 2;
            XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

            position = XMVector3Transform(position, transform);
            normal = XMVector3TransformNormal(normal, transform);

            vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(position, normal, textureCoordinate));

            // And create indices for two triangles.
            size_t nextI = (i + 1) % stride;
            size_t nextJ = (j + 1) % stride;

            indices.emplace_back(static_cast<uint16_t>(i * stride + j));
            indices.emplace_back(static_cast<uint16_t>(i * stride + nextJ));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + j));

            indices.emplace_back(static_cast<uint16_t>(i * stride + nextJ));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + nextJ));
            indices.emplace_back(static_cast<uint16_t>(nextI * stride + j));
        }
    }

    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
}

void Mesh::InitializeAsPlane(float width, float height)
{
    /*
    vertices.reserve(4);
    indices.reserve(6);

    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f)));
    
    indices.emplace_back(0);
    indices.emplace_back(3);
    indices.emplace_back(1);
    indices.emplace_back(1);
    indices.emplace_back(3);
    indices.emplace_back(2);
    
    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
    */

    vertices.reserve(3);
    indices.reserve(3);

    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(0.5f * width, 0.0f, 0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f)));

    indices.emplace_back(0);
    indices.emplace_back(1);
    indices.emplace_back(2);

    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    //ReverseWinding(indices, vertices);
}

void Mesh::InitializeAsVerticalPlane(float width, float height)
{
    vertices.reserve(4);
    indices.reserve(6);

    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, 0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(0.5f * width, 0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(0.5f * width, -0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f)));
    vertices.emplace_back(ShaderStructureCPUVertexPositionNormalTextureColor(XMFLOAT3(-0.5f * width, -0.5f * height, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f)));

    indices.emplace_back(0);
    indices.emplace_back(3);
    indices.emplace_back(1);
    indices.emplace_back(1);
    indices.emplace_back(3);
    indices.emplace_back(2);

    // Make sure to invert the indices and vertices winding orders, to match
    // directX's left hand coordinate system
    ReverseWinding(indices, vertices);
}

void Mesh::SetColor(FXMVECTOR& color)
{
    for (int i = 0; i < vertices.size(); i++)
    {
        XMStoreFloat3(&vertices[i].color, color);
    }
}

void Mesh::ReverseWinding(vector<uint16_t>& indices, vector<ShaderStructureCPUVertexPositionNormalTextureColor>& vertices)
{
    assert((indices.size() % 3) == 0);
    for (auto it = indices.begin(); it != indices.end(); it += 3)
    {
        std::swap(*it, *(it + 2));
    }

    for (auto it = vertices.begin(); it != vertices.end(); ++it)
    {
        it->textureCoordinate.x = (1.f - it->textureCoordinate.x);
    }
}

void Mesh::SetIsStatic(bool isStatic)
{
    is_static = isStatic;
}