/*
 *  Copyright (C) 2007 Austin Robot Technology, Patrick Beeson
 *  Copyright (C) 2009, 2010, 2012 Austin Robot Technology, Jack O'Quin
 *	Copyright (C) 2017 Robosense, Tony Zhang
 *
 *  License: Modified BSD Software License Agreement
 *
 *  $Id$
 */

/**
 *  @file
 *
 *  RSLIDAR 3D LIDAR data accessor class implementation.
 *
 *  Class for unpacking raw RSLIDAR LIDAR packets into useful
 *  formats.
 *
 */
#include "rawdata.h"

namespace rslidar_rawdata
{

RawData::RawData(){ }

void RawData::loadConfigFile(ros::NodeHandle private_nh)
{

  std::string anglePath, curvesPath;
  std::string channelPath;


  private_nh.param("curves_path", curvesPath, std::string(""));
  private_nh.param("angle_path", anglePath, std::string(""));
  private_nh.param("channel_path", channelPath, std::string(""));


  /// 读参数文件 2017-02-27
  FILE *f_inten = fopen(curvesPath.c_str(), "r");
  int loopi = 0;
  int loopj = 0;

  if(!f_inten)
  {
    ROS_ERROR_STREAM(curvesPath << " does not exist");
  }
  else
  {
    while(~feof(f_inten))
    {
      float a[16];
      loopi++;
      if(loopi > 1600)
        break;
      fscanf(f_inten, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
             &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10], &a[11], &a[12], &a[13],
             &a[14], &a[15]);
      for(loopj = 0; loopj < 16; loopj++)
      {
        aIntensityCal[loopi - 1][loopj] = a[loopj];
      }
    }
    fclose(f_inten);
  }
  //=============================================================
  FILE *f_angle = fopen(anglePath.c_str(), "r");
  if(!f_angle)
  {
    ROS_ERROR_STREAM(anglePath << " does not exist");
  }
  else
  {
    float b[16];
    int loopk = 0;
    int loopn = 0;
    while(~feof(f_angle))
    {
      fscanf(f_angle, "%f", &b[loopk]);
      loopk++;
      if(loopk > 15) break;
    }
    for(loopn = 0; loopn < 16; loopn++)
    {
      VERT_ANGLE[loopn] = b[loopn] / 180 * M_PI;
    }
    fclose(f_angle);
  }

  //=============================================================
  FILE *f_channel = fopen(channelPath.c_str(),"r");
  if(!f_channel)
  {
      ROS_ERROR_STREAM(channelPath << " does not exist");
  }
  else
  {
      printf("Loading channelnum corrections file!\n");
      int loopl=0;
      int loopm=0;
      int c[41];
      while (~feof(f_channel))
      {
        fscanf(f_channel, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
         &c[0], &c[1], &c[2], &c[3], &c[4], &c[5], &c[6], &c[7], &c[8], &c[9], &c[10], &c[11], &c[12], &c[13], &c[14], &c[15], 
         &c[16], &c[17], &c[18], &c[19], &c[20], &c[21], &c[22], &c[23], &c[24], &c[25], &c[26], &c[27], &c[28], &c[29], &c[30],
         &c[31], &c[32], &c[33], &c[34], &c[35], &c[36], &c[37], &c[38], &c[39], &c[40]);

        if(c[1]<100||c[1]>1000)//TODO:2100 is not the real max value
        {
          for (loopl = 0; loopl < 41; loopl++)
          {
            g_ChannelNum[loopm][loopl] =c[0];
            //printf("Non temperature information in correction file .\n");
          }
        }
        else
        {
          for (loopl = 0; loopl < 41; loopl++)
          {
            g_ChannelNum[loopm][loopl] =c[loopl];
            //printf("With temperature information in correction file.\n");
          }
        }
        loopm++;
        if(loopm>15)
          {break;}
        
      }
      fclose(f_channel);
   }
}

/** Set up for on-line operation. */
void RawData::init_setup()
{
  pic.col = 0;
  pic.azimuth.resize(POINT_PER_CIRCLE_);
  pic.distance.resize(DATA_NUMBER_PER_SCAN);
  pic.intensity.resize(DATA_NUMBER_PER_SCAN);
  pic.azimuthforeachP.resize(DATA_NUMBER_PER_SCAN);
}

float RawData::pixelToDistance(int pixelValue, int passageway)
{
  float DistanceValue;

  int indexTemper = estimateTemperature(temper)-30;
  if(pixelValue <= g_ChannelNum[passageway][indexTemper])
  {
    DistanceValue = 0.0;
  }
  else
  {
    DistanceValue = (float) (pixelValue - g_ChannelNum[passageway][indexTemper]);
  }
  return DistanceValue;
}

//校准反射强度值
float RawData::calibrateIntensity(float intensity, int calIdx, int distance)
{
  int algDist;
  int sDist;
  int uplimitDist;
  float realPwr;
  float refPwr;
  float tempInten;

  int indexTemper = estimateTemperature(temper)-30;
  uplimitDist = g_ChannelNum[calIdx][indexTemper] + 1400;
  realPwr = intensity;

  if((int) realPwr < 126)
    realPwr = realPwr * 4.0;
  else if((int) realPwr >= 126 && (int) realPwr < 226)
    realPwr = (realPwr - 125.0) * 16.0 + 500.0;
  else
    realPwr = (realPwr - 225.0) * 256.0 + 2100.0;



  sDist = (distance > g_ChannelNum[calIdx][indexTemper]) ? distance : g_ChannelNum[calIdx][indexTemper];
  sDist = (sDist < uplimitDist) ? sDist : uplimitDist;
  //minus the static offset (this data is For the intensity cal useage only)
  algDist = sDist - g_ChannelNum[calIdx][indexTemper];
  //algDist = algDist < 1400? algDist : 1399;
  refPwr = aIntensityCal[algDist][calIdx];
  tempInten = (200 * refPwr) / realPwr;
  //tempInten = tempInten * 200.0;
  tempInten = (int) tempInten > 255 ? 255.0 : tempInten;
  return tempInten;
  //------------------------------------------------------------
}

//------------------------------------------------------------
float RawData::computeTemperature(
  unsigned char bit1, unsigned char bit2)
{
  float Temp;
  float bitneg = bit2 & 128;//10000000
  float highbit = bit2 & 127;//01111111
  float lowbit= bit1 >> 3;
  if(bitneg == 128)
  {
    Temp = -1 *(highbit * 32 + lowbit)*0.0625;
  }
  else
  {
    Temp = (highbit * 32 + lowbit)*0.0625;
  }

  return Temp;
}
//------------------------------------------------------------
int RawData::estimateTemperature(float Temper)
{
  int temp = floor(Temper+0.5);

  if(temp<30)
  {
    temp = 30;
  }
  else if(temp>70)
  {
    temp = 70;
  }

  return temp;
}
//------------------------------------------------------------

/** @brief convert raw packet to point cloud
 *
 *  @param pkt raw packet to unpack
 *  @param pc shared pointer to point cloud (points are appended)
 */
void RawData::unpack(const rslidar_msgs::rslidarPacket &pkt, pcl::PointCloud<pcl::PointXYZI>::Ptr pointcloud,
                     bool finish_packets_parse)
{
  float azimuth;  //0.01 dgree
  float intensity;
  float azimuth_diff;
  float last_azimuth_diff;
  float azimuth_corrected_f;
  int azimuth_corrected;

  const raw_packet_t *raw = (const raw_packet_t *) &pkt.data[42];

  for(int block = 0; block < BLOCKS_PER_PACKET; block++) //1 packet:12 data blocks
  {

    if(UPPER_BANK != raw->blocks[block].header)
    {
      ROS_INFO_STREAM_THROTTLE(180, "skipping RSLIDAR DIFOP packet");


      break;
    }

    if(tempPacketNum<20000 && tempPacketNum>0)//update temperature information per 20000 packets
    {
        tempPacketNum++;
    }
    else
    {
        temper = computeTemperature(pkt.data[38],pkt.data[39]);
        tempPacketNum = 1;
    }

    azimuth = (float) (256 * raw->blocks[block].rotation_1 + raw->blocks[block].rotation_2);

    if(block < (BLOCKS_PER_PACKET - 1))//12
    {
      int azi1, azi2;
      azi1 = 256 * raw->blocks[block + 1].rotation_1 + raw->blocks[block + 1].rotation_2;
      azi2 = 256 * raw->blocks[block].rotation_1 + raw->blocks[block].rotation_2;
      azimuth_diff = (float) ((36000 + azi1 - azi2) % 36000);
      //Debug start by Tony 20170523
      if(azimuth_diff <= 0.0 || azimuth_diff > 70.0)
      {
        //ROS_INFO("Error: %d  %d", azi1, azi2);
        azimuth_diff = 40.0;
        azimuth = pic.azimuth[pic.col - 1] + azimuth_diff;
      }
      //Debug end by Tony 20170523
      last_azimuth_diff = azimuth_diff;
    }
    else
    {
      azimuth_diff = last_azimuth_diff;
    }

    for(int firing = 0, k = 0; firing < RS16_FIRINGS_PER_BLOCK; firing++)//2
    {
      for(int dsr = 0; dsr < RS16_SCANS_PER_FIRING; dsr++, k += RAW_SCAN_SIZE)//16   3
      {
        int point_count = pic.col * SCANS_PER_BLOCK + dsr + RS16_SCANS_PER_FIRING * firing;
        azimuth_corrected_f = azimuth + (azimuth_diff * ((dsr * RS16_DSR_TOFFSET) + (firing * RS16_FIRING_TOFFSET)) /
                                         RS16_BLOCK_TDURATION);
        azimuth_corrected = ((int) round(azimuth_corrected_f)) % 36000;//convert to integral value...
        pic.azimuthforeachP[point_count] = azimuth_corrected;

        union two_bytes tmp;
        tmp.bytes[1] = raw->blocks[block].data[k];
        tmp.bytes[0] = raw->blocks[block].data[k + 1];
        int distance = tmp.uint;

        // read intensity
        intensity = raw->blocks[block].data[k + 2];
        intensity = calibrateIntensity(intensity, dsr, distance);

        float distance2 = pixelToDistance(distance, dsr);
        distance2 = distance2 * DISTANCE_RESOLUTION;

        pic.distance[point_count] = distance2;
        pic.intensity[point_count] = intensity;
      }
    }
    pic.azimuth[pic.col] = azimuth;
    pic.col++;
  }

  if(finish_packets_parse)
  {
    //ROS_INFO_STREAM("***************: "<<pic.col);
    pointcloud->clear();
    pointcloud->height = RS16_SCANS_PER_FIRING;
    pointcloud->width = 2 * pic.col;
    pointcloud->is_dense = false;
    pointcloud->resize(pointcloud->height * pointcloud->width);
    mat_depth = cv::Mat(cv::Size(2 * pic.col, RS16_SCANS_PER_FIRING), CV_32FC1, cv::Scalar(0));
    //mat_inten = cv::Mat(cv::Size(2 * pic.col ,RS16_SCANS_PER_FIRING ), CV_8UC1, cv::Scalar(0));
    for(int block_num = 0; block_num < pic.col; block_num++)
    {

      for(int firing = 0; firing < RS16_FIRINGS_PER_BLOCK; firing++)
      {
        for(int dsr = 0; dsr < RS16_SCANS_PER_FIRING; dsr++)
        {
          int point_count = block_num * SCANS_PER_BLOCK + dsr + RS16_SCANS_PER_FIRING * firing;
          float dis = pic.distance[point_count];
          float arg_horiz = pic.azimuthforeachP[point_count] / 18000 * M_PI;
          float intensity = pic.intensity[point_count];
          float arg_vert = VERT_ANGLE[dsr];
          pcl::PointXYZI point;
          if(dis > DISTANCE_MAX || dis < DISTANCE_MIN)  //invalid data
          {
            point.x = NAN;
            point.y = NAN;
            point.z = NAN;
            point.intensity = 0;
            //ROS_INFO("inten: %f", intensity);
            //pointcloud->push_back(point);
            pointcloud->at(2 * block_num + firing, dsr) = point;
            mat_depth.at<float>(dsr, 2 * block_num + firing) = 0;
            //mat_inten.at<uchar>(dsr,2*block_num + firing) = (uchar)0;
          }
          else
          {
            //If you want to fix the rslidar Y aixs to the front side of the cable, please use the two line below
            //point.x = dis * cos(arg_vert) * sin(arg_horiz);
            //point.y = dis * cos(arg_vert) * cos(arg_horiz);

            //If you want to fix the rslidar X aixs to the front side of the cable, please use the two line below
            point.y = -dis * cos(arg_vert) * sin(arg_horiz);
            point.x = dis * cos(arg_vert) * cos(arg_horiz);
            point.z = dis * sin(arg_vert);
            point.intensity = intensity;
            pointcloud->at(2 * block_num + firing, dsr) = point;
            mat_depth.at<float>(dsr, 2 * block_num + firing) = dis;
            //mat_inten.at<uchar>(dsr,2*block_num + firing) = (uchar)intensity;

          }
        }
      }
    }
    removeOutlier(pointcloud); //去除脏数据
    init_setup();
    pic.header.stamp = pkt.stamp;
  }
}



void regionGrow(cv::Mat src, cv::Mat &matDst, cv::Point2i pt, float th)
{

  cv::Point2i ptGrowing;
  int nGrowLable = 0;
  float nSrcValue = 0;
  float nCurValue = 0;


  int DIR[10][2] = {{-1, 0},
                    {1,  0},
                    {-2, 0},
                    {2,  0},
                    {-3, 0},
                    {3,  0},
                    {-4, 0},
                    {4,  0},
                    {0,  1},
                    {0,  -1}};


  nSrcValue = src.at<float>(pt);
  matDst.at<uchar>(pt) = 0;
  if(nSrcValue == 0)
  {
    matDst.at<uchar>(pt) = 255;
    return;
  }

  //分别对八个方向上的点进行生长
  for(int i = 0; i < 10; ++i)
  {
    ptGrowing.x = pt.x + DIR[i][0];
    ptGrowing.y = pt.y + DIR[i][1];
    //检查是否是边缘点
    if(ptGrowing.x < 0 || ptGrowing.y<0 || ptGrowing.x>(src.cols - 1) || (ptGrowing.y > src.rows - 1))
      continue;


    nCurValue = src.at<float>(ptGrowing);

    if(nCurValue == 0)
    {
      continue;
    }

    if(nSrcValue <= 2)//在阈值范围内则生长
    {
      if(abs(nSrcValue - nCurValue) < 0.2)
      {
        matDst.at<uchar>(pt) += 1;
      }

      if(matDst.at<uchar>(pt) >= 3)
      {
        matDst.at<uchar>(pt) = 255;
        return;
      }
    }
    if(nSrcValue > 2 && nSrcValue <= 5)
    {
      if(abs(nSrcValue - nCurValue) < 0.5)
      {
        matDst.at<uchar>(pt) += 1;
      }

      if(matDst.at<uchar>(pt) >= 2)
      {
        matDst.at<uchar>(pt) = 255;
        return;
      }
    }
    if(nSrcValue > 5)
    {

      if(abs(nSrcValue - nCurValue) < 1)
      {
        matDst.at<uchar>(pt) += 1;
      }

      if(matDst.at<uchar>(pt) >= 1)
      {
        matDst.at<uchar>(pt) = 255;
        return;
      }
    }
  }
}


void removeOutlier(pcl::PointCloud<pcl::PointXYZI>::Ptr pointcloud)
{

  cv::Mat matMask = cv::Mat::zeros(mat_depth.size(), CV_8UC1);
  for(int row = 0; row < mat_depth.rows; row++)
  {
    for(int col = 0; col < mat_depth.cols; col++)
    {
      float d = mat_depth.at<float>(row, col);
      if(d > 8)
      {
        matMask.at<uchar>(row, col) = (uchar) 255;
        continue;
      }

      cv::Point2i pt(col, row);
      regionGrow(mat_depth, matMask, pt, 1);
    }
  }

  for(int row = 0; row < matMask.rows; row++)
  {
    for(int col = 0; col < matMask.cols; col++)
    {
      if(matMask.at<uchar>(row, col) != 255)
      {

        pointcloud->at(col, row).x = NAN;
        pointcloud->at(col, row).y = NAN;
        pointcloud->at(col, row).z = NAN;

      }
    }
  }
}
}//namespace rs_pointcloud


