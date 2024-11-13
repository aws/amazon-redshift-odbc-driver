﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshift/Redshift_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/redshift/model/ResponseMetadata.h>
#include <aws/redshift/model/SnapshotSchedule.h>
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
  class DescribeSnapshotSchedulesResult
  {
  public:
    AWS_REDSHIFT_API DescribeSnapshotSchedulesResult();
    AWS_REDSHIFT_API DescribeSnapshotSchedulesResult(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);
    AWS_REDSHIFT_API DescribeSnapshotSchedulesResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);


    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline const Aws::Vector<SnapshotSchedule>& GetSnapshotSchedules() const{ return m_snapshotSchedules; }

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline void SetSnapshotSchedules(const Aws::Vector<SnapshotSchedule>& value) { m_snapshotSchedules = value; }

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline void SetSnapshotSchedules(Aws::Vector<SnapshotSchedule>&& value) { m_snapshotSchedules = std::move(value); }

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline DescribeSnapshotSchedulesResult& WithSnapshotSchedules(const Aws::Vector<SnapshotSchedule>& value) { SetSnapshotSchedules(value); return *this;}

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline DescribeSnapshotSchedulesResult& WithSnapshotSchedules(Aws::Vector<SnapshotSchedule>&& value) { SetSnapshotSchedules(std::move(value)); return *this;}

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline DescribeSnapshotSchedulesResult& AddSnapshotSchedules(const SnapshotSchedule& value) { m_snapshotSchedules.push_back(value); return *this; }

    /**
     * <p>A list of SnapshotSchedules.</p>
     */
    inline DescribeSnapshotSchedulesResult& AddSnapshotSchedules(SnapshotSchedule&& value) { m_snapshotSchedules.push_back(std::move(value)); return *this; }


    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline const Aws::String& GetMarker() const{ return m_marker; }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline void SetMarker(const Aws::String& value) { m_marker = value; }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline void SetMarker(Aws::String&& value) { m_marker = std::move(value); }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline void SetMarker(const char* value) { m_marker.assign(value); }

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline DescribeSnapshotSchedulesResult& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline DescribeSnapshotSchedulesResult& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    /**
     * <p>A value that indicates the starting point for the next set of response
     * records in a subsequent request. If a value is returned in a response, you can
     * retrieve the next set of records by providing this returned marker value in the
     * <code>marker</code> parameter and retrying the command. If the
     * <code>marker</code> field is empty, all response records have been retrieved for
     * the request.</p>
     */
    inline DescribeSnapshotSchedulesResult& WithMarker(const char* value) { SetMarker(value); return *this;}


    
    inline const ResponseMetadata& GetResponseMetadata() const{ return m_responseMetadata; }

    
    inline void SetResponseMetadata(const ResponseMetadata& value) { m_responseMetadata = value; }

    
    inline void SetResponseMetadata(ResponseMetadata&& value) { m_responseMetadata = std::move(value); }

    
    inline DescribeSnapshotSchedulesResult& WithResponseMetadata(const ResponseMetadata& value) { SetResponseMetadata(value); return *this;}

    
    inline DescribeSnapshotSchedulesResult& WithResponseMetadata(ResponseMetadata&& value) { SetResponseMetadata(std::move(value)); return *this;}

  private:

    Aws::Vector<SnapshotSchedule> m_snapshotSchedules;

    Aws::String m_marker;

    ResponseMetadata m_responseMetadata;
  };

} // namespace Model
} // namespace Redshift
} // namespace Aws
