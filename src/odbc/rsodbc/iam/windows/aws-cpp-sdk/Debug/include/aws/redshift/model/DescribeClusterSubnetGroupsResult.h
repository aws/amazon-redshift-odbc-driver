﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshift/Redshift_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/redshift/model/ResponseMetadata.h>
#include <aws/redshift/model/ClusterSubnetGroup.h>
#include <utility>

namespace Aws
{
template<typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils
{
namespace Xml
{
  class XmlDocument;
} // namespace Xml
} // namespace Utils
namespace Redshift
{
namespace Model
{
  /**
   * <p>Contains the output from the <a>DescribeClusterSubnetGroups</a> action.
   * </p><p><h3>See Also:</h3>   <a
   * href="http://docs.aws.amazon.com/goto/WebAPI/redshift-2012-12-01/ClusterSubnetGroupMessage">AWS
   * API Reference</a></p>
   */
  class DescribeClusterSubnetGroupsResult
  {
  public:
    AWS_REDSHIFT_API DescribeClusterSubnetGroupsResult();
    AWS_REDSHIFT_API DescribeClusterSubnetGroupsResult(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);
    AWS_REDSHIFT_API DescribeClusterSubnetGroupsResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);


    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline const Aws::String& GetMarker() const{ return m_marker; }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline void SetMarker(const Aws::String& value) { m_marker = value; }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline void SetMarker(Aws::String&& value) { m_marker = std::move(value); }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline void SetMarker(const char* value) { m_marker.assign(value); }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline DescribeClusterSubnetGroupsResult& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline DescribeClusterSubnetGroupsResult& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>Marker</code> parameter and retrying the command. If the
     * <code>Marker</code> field is empty, all response records have been retrieved for
     * the request. </p>
     */
    inline DescribeClusterSubnetGroupsResult& WithMarker(const char* value) { SetMarker(value); return *this;}


    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline const Aws::Vector<ClusterSubnetGroup>& GetClusterSubnetGroups() const{ return m_clusterSubnetGroups; }

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline void SetClusterSubnetGroups(const Aws::Vector<ClusterSubnetGroup>& value) { m_clusterSubnetGroups = value; }

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline void SetClusterSubnetGroups(Aws::Vector<ClusterSubnetGroup>&& value) { m_clusterSubnetGroups = std::move(value); }

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline DescribeClusterSubnetGroupsResult& WithClusterSubnetGroups(const Aws::Vector<ClusterSubnetGroup>& value) { SetClusterSubnetGroups(value); return *this;}

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline DescribeClusterSubnetGroupsResult& WithClusterSubnetGroups(Aws::Vector<ClusterSubnetGroup>&& value) { SetClusterSubnetGroups(std::move(value)); return *this;}

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline DescribeClusterSubnetGroupsResult& AddClusterSubnetGroups(const ClusterSubnetGroup& value) { m_clusterSubnetGroups.push_back(value); return *this; }

    /**
     * <p>A list of <a>ClusterSubnetGroup</a> instances. </p>
     */
    inline DescribeClusterSubnetGroupsResult& AddClusterSubnetGroups(ClusterSubnetGroup&& value) { m_clusterSubnetGroups.push_back(std::move(value)); return *this; }


    
    inline const ResponseMetadata& GetResponseMetadata() const{ return m_responseMetadata; }

    
    inline void SetResponseMetadata(const ResponseMetadata& value) { m_responseMetadata = value; }

    
    inline void SetResponseMetadata(ResponseMetadata&& value) { m_responseMetadata = std::move(value); }

    
    inline DescribeClusterSubnetGroupsResult& WithResponseMetadata(const ResponseMetadata& value) { SetResponseMetadata(value); return *this;}

    
    inline DescribeClusterSubnetGroupsResult& WithResponseMetadata(ResponseMetadata&& value) { SetResponseMetadata(std::move(value)); return *this;}

  private:

    Aws::String m_marker;

    Aws::Vector<ClusterSubnetGroup> m_clusterSubnetGroups;

    ResponseMetadata m_responseMetadata;
  };

} // namespace Model
} // namespace Redshift
} // namespace Aws
