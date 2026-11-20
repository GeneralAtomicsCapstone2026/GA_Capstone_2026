#!/usr/bin/env python
# encoding: utf-8
import math
import time
import cv2 as cv
import numpy as np
from typing import NamedTuple
from std_msgs.msg import Bool
from geometry_msgs.msg import Twist

class LineCentroid(NamedTuple):
  width: int      # in samples (~px along the arc, px for a row)
  x: int          # center pixel of the line, image coordinates
  y: int
  heading: float  # radians from straight ahead, seen from the car (bottom-middle of the frame).
                  # + is left, - is right

# Frame size after resizing, used for precomputed arc pixel maps
FRAME_WIDTH = 640
FRAME_HEIGHT = 480
# Fraction of the image height (from the top) that is blacked out before line detection.
# 0.0 = use the whole frame, 0.5 = ignore the top half, 1.0 = ignore everything.
LINE_FOLLOW_TOP_CROP_FRACTION = 0.2
# Sample a linear band of pixels of this thickness
LINE_SAMPLE_BAND_HEIGHT = 10
# A column of the sampled band (linear or radial) counts as "line"
# if at least this fraction of its pixels are lit
SAMPLE_BAND_CONFIDENCE = 0.5
# blobs of this size and larger are considered significant
GAP_THRESH = 5
LINE_THRESH = 5
# Sample a radial band of pixels at this radius
ARC_SAMPLE_RADIUS = 275
# Band thickness in px, centered on the above radius
ARC_SAMPLE_THICKNESS = 9

def find_lines(px_arr):
    """Find the lines (runs of 1s) in a 1D 0/1 array.

    Gaps narrower than GAP_THRESH are bridged first, then lines narrower than
    LINE_THRESH are dropped. Returns a list of (start, end) index pairs, end exclusive.
    """
    # int8 so np.diff gives -1 at line ends (uint8 would wrap to 255)
    arr = (np.asarray(px_arr) > 0).astype(np.int8)
    diff_arr = np.diff(np.pad(arr, 1))
    # start and stop indices of LINES, not GAPS
    starts = np.flatnonzero(diff_arr == 1).tolist()
    ends = np.flatnonzero(diff_arr == -1).tolist()

    # REMOVE INSIGNIFICANT GAPS
    # iterate in reverse to avoid indexing issues when deleting
    for i in range(len(starts) - 2, -1, -1):
        if starts[i+1] - ends[i] < GAP_THRESH:
            del starts[i+1]
            del ends[i]

    # REMOVE INSIGNIFICANT LINES
    return [(start, end) for start, end in zip(starts, ends) if end - start >= LINE_THRESH]


def heading_to(x, y, origin_x, origin_y):
    """Angle from straight ahead (image up) to point (x, y), seen from the origin.
    Radians, + = left, - = right."""
    return math.atan2(origin_x - x, origin_y - y)


def build_arc_maps(width, height, radius, thickness):
    """Precompute the cv.remap maps for the arc band.

    Returns (map_x, map_y, theta): float32 maps of shape (thickness, samples), and the
    angle of each sample (pi = left ... 0 = right).
    """
    cx, cy = width / 2, height - 1
    n_samples = int(round(np.pi * radius))
    theta = np.linspace(np.pi, 0, n_samples)
    radii = radius - thickness // 2 + np.arange(thickness)
    map_x = (cx + radii[:, None] * np.cos(theta)).astype(np.float32)
    map_y = (cy - radii[:, None] * np.sin(theta)).astype(np.float32)
    return map_x, map_y, theta

def write_HSV(wf_path, value):
    with open(wf_path, "w") as wf:
        wf_str = str(value[0][0]) + ', ' + str(
            value[0][1]) + ', ' + str(value[0][2]) + ', ' + str(
            value[1][0]) + ', ' + str(value[1][1]) + ', ' + str(
            value[1][2])
        wf.write(wf_str)
        wf.flush()

def read_HSV(rf_path):
    rf = open(rf_path, "r+")
    line = rf.readline()
    if len(line) == 0: return ()
    list = line.split(',')
    if len(list) != 6: return ()
    hsv = ((int(list[0]), int(list[1]), int(list[2])),
           (int(list[3]), int(list[4]), int(list[5])))
    rf.flush()
    return hsv


# Define the function, the first parameter is the zoom ratio, and the second parameter is a tuple or list of pictures to be displayed
def ManyImgs(scale, imgarray):
    rows = len(imgarray)  # Length of tuple or list
    cols = len(imgarray[0])
    # If imgarray is a list, return the number of channels of the first image in the list, if it is a tuple, return the length of the first list contained in the tuple

    # print("rows=", rows, "cols=", cols)
    # Determine whether the type of imgarray[0] is list
    # It is a list, indicating that imgarray is a tuple and needs to be displayed verticallyIt is a list, indicating that imgarray is a tuple and needs to be displayed vertically
    rowsAvailable = isinstance(imgarray[0], list)
    # The width and height of the first picture
    width = imgarray[0][0].shape[1]
    height = imgarray[0][0].shape[0]
    # print("width=", width, "height=", height)
    # If the incoming is a tuple
    if rowsAvailable:
        for x in range(0, rows):
            for y in range(0, cols):
                # Traverse the tuple, if it is the first image, do not transform
                if imgarray[x][y].shape[:2] == imgarray[0][0].shape[:2]:
                    imgarray[x][y] = cv.resize(imgarray[x][y], (0, 0), None, scale, scale)
                # Transform other matrices to the same size as the first image, and the zoom ratio is scale
                else:
                    imgarray[x][y] = cv.resize(imgarray[x][y], (imgarray[0][0].shape[1], imgarray[0][0].shape[0]), None,
                                               scale, scale)
                # If the image is grayscale, convert it to color display
                if len(imgarray[x][y].shape) == 2:
                    imgarray[x][y] = cv.cvtColor(imgarray[x][y], cv.COLOR_GRAY2BGR)
        # Create a blank canvas, the same size as the first picture
        imgBlank = np.zeros((height, width, 3), np.uint8)
        hor = [imgBlank] * rows
        # The same size as the first picture, and the same number of horizontal blank images as the tuple contains the list
        for x in range(0, rows):
            # 将元组里第x个列表水平排列
            # Arrange the x-th list in the tuple horizontally
            hor[x] = np.hstack(imgarray[x])
        ver = np.vstack(hor)
        # Concatenate different lists vertically
    # If the incoming is a list
    else:
        # Transformation operation, same as before
        for x in range(0, rows):
            if imgarray[x].shape[:2] == imgarray[0].shape[:2]:
                imgarray[x] = cv.resize(imgarray[x], (0, 0), None, scale, scale)
            else:
                imgarray[x] = cv.resize(imgarray[x], (imgarray[0].shape[1], imgarray[0].shape[0]), None, scale, scale)
            if len(imgarray[x].shape) == 2:
                imgarray[x] = cv.cvtColor(imgarray[x], cv.COLOR_GRAY2BGR)
        # Arrange the list horizontally
        hor = np.hstack(imgarray)
        ver = hor
    return ver


class color_follow:
    def __init__(self, arc_radius=ARC_SAMPLE_RADIUS, arc_thickness=ARC_SAMPLE_THICKNESS,
                 frame_size=(FRAME_WIDTH, FRAME_HEIGHT)):
        '''
	    Initialize some parameters
        '''
        self.binary = None
        self.Center_x = 0
        self.Center_y = 0
        self.Center_r = 0
        self.arc_lines = []
        self.arc_radius = arc_radius
        self.arc_thickness = arc_thickness
        # Precompute the arc remap maps once instead of per frame
        self.init_arc_maps(*frame_size)

    def init_arc_maps(self, width, height):
        self.arc_frame_shape = (height, width)
        self.arc_map_x, self.arc_map_y, self.arc_theta = build_arc_maps(
            width, height, self.arc_radius, self.arc_thickness)

    def sample_arc(self, binary):
        """Sample the arc band of the binary image and return the lines crossing it.
        """
        if binary.shape[:2] != self.arc_frame_shape:
            # Only happens if the frame size changes (process() always resizes to 640x480)
            self.init_arc_maps(binary.shape[1], binary.shape[0])
        # Translates radial band (arc) into a linear band that numpy can use easily
        band = cv.remap(binary, self.arc_map_x, self.arc_map_y, cv.INTER_NEAREST,
                        borderMode=cv.BORDER_CONSTANT, borderValue=0)
        # Only consider arc pixels whose column is more than 50% lit - reduces noise 
        arc = ((band > 0).mean(axis=0) >= SAMPLE_BAND_CONFIDENCE).astype(np.uint8)

        img_h, img_w = self.arc_frame_shape
        cx, cy = img_w / 2, img_h - 1
        lines = []
        for start, end in find_lines(arc):
            theta = 0.5 * (self.arc_theta[start] + self.arc_theta[end - 1])
            x = cx + self.arc_radius * math.cos(theta)
            y = cy - self.arc_radius * math.sin(theta)
            lines.append(LineCentroid(end - start, int(round(x)), int(round(y)),
                                      heading_to(x, y, cx, cy)))
        return lines

    def sample_row(self, binary, center_y):
        """Sample a horizontal band of the binary image and return the lines crossing it.

        Does not modify binary. Lines are ordered left to right; indices are columns.
        """
        img_h, img_w = binary.shape[:2]
        cx, cy = img_w / 2, img_h - 1
        lines = []
        for start, end in find_lines(self.collapse_band(binary, center_y)):
            x = 0.5 * (start + end - 1)
            lines.append(LineCentroid(end - start, int(round(x)), int(center_y),
                                      heading_to(x, center_y, cx, cy)))
        return lines

    def collapse_band(self, binary, center_y):
        """Collapse a horizontal band of the binary image into a 1D 0/1 row.
        The band is LINE_SAMPLE_BAND_HEIGHT rows tall, centered on center_y (clamped to the
        image). A column is 1 if at least LINE_SAMPLE_BAND_CONFIDENCE of the band's pixels
        in that column are lit.
        """
        img_h = binary.shape[0]
        band_h = min(LINE_SAMPLE_BAND_HEIGHT, img_h)
        top = max(0, min(center_y - band_h // 2, img_h - band_h))
        band = binary[top:top + band_h] > 0
        min_count = int(np.ceil(SAMPLE_BAND_CONFIDENCE * band_h))
        return (band.sum(axis=0) >= min_count).astype(np.uint8)

    def line_follow(self, rgb_img, hsv_msg):
        height, width = rgb_img.shape[:2]
        img = rgb_img.copy()
        img[0:int(height * LINE_FOLLOW_TOP_CROP_FRACTION), 0:width] = 0
        # Convert the image to HSV.
        hsv_img = cv.cvtColor(img, cv.COLOR_BGR2HSV)
        lower = np.array(hsv_msg[0], dtype="uint8")
        upper = np.array(hsv_msg[1], dtype="uint8")
        # Create a mask based on a specific color range
        mask = cv.inRange(hsv_img, lower, upper)
        color_mask = cv.bitwise_and(hsv_img, hsv_img, mask=mask)
        # Convert the image to grayscale
        gray_img = cv.cvtColor(color_mask, cv.COLOR_RGB2GRAY)
        # Get structure elements of different shapes
        kernel = cv.getStructuringElement(cv.MORPH_RECT, (5, 5))
        # Morphological closed operation
        gray_img = cv.morphologyEx(gray_img, cv.MORPH_CLOSE, kernel)
        # Image binarization operation
        ret, binary = cv.threshold(gray_img, 10, 255, cv.THRESH_BINARY)
        # Sample one arc band around the car and detect the lines crossing it
        # (more than one line = fork/divergence)
        self.arc_lines = self.sample_arc(binary)
        # Get the set of contour points (coordinates)
        find_contours = cv.findContours(binary, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)
        if len(find_contours) == 3: contours = find_contours[1]
        else: contours = find_contours[0]
        if len(contours) != 0:
            areas = []
            for c in range(len(contours)): areas.append(cv.contourArea(contours[c]))
            max_id = areas.index(max(areas))
            max_rect = cv.minAreaRect(contours[max_id])
            max_box = cv.boxPoints(max_rect)
            # Convert box vertices to integer pixel coordinates
            max_box = np.int0(max_box)
            # Draw the smallest rectangle
            cv.drawContours(rgb_img, [max_box], 0, (255, 0, 0), 2)
            (color_x, color_y), color_radius = cv.minEnclosingCircle(max_box)
            # Mark the detected color with the original shape coil
            self.Center_x = int(color_x)
            self.Center_y = int(color_y)
            self.Center_r = int(color_radius)
            cv.circle(rgb_img, (self.Center_x, self.Center_y), 5, (255, 0, 255), -1)
        else:
            self.Center_x = 0
            self.Center_y = 0
            self.Center_r = 0
        return rgb_img, binary, (self.Center_x, self.Center_y, self.Center_r), self.arc_lines

    def Roi_hsv(self, img, Roi):
        '''
        Get the range of HSV in a certain area
        :param img:
        :param Roi:  (x_min, y_min, x_max, y_max)
        Roi=(290,280,350,340)
        :return: The range of images and HSV such as:(0,0,90)(177,40,150)
        '''
        H = [];S = [];V = []
        HSV = cv.cvtColor(img, cv.COLOR_BGR2HSV)

        # Take out the H, S, V values of each row and each column in turn and put them into the container
        for i in range(Roi[0], Roi[2]):
            for j in range(Roi[1], Roi[3]):
                H.append(HSV[j, i][0])
                S.append(HSV[j, i][1])
                V.append(HSV[j, i][2])
        # Calculate the maximum and minimum of H, S, and V respectively
        H_min = min(H); H_max = max(H)
        S_min = min(S); S_max = 253
        V_min = min(V); V_max = 255
        # HSV range adjustment
        if H_max + 5 > 255: H_max = 255
        else: H_max += 5
        if H_min - 5 < 0: H_min = 0
        else: H_min -= 5
        if S_min - 20 < 0: S_min = 0
        else: S_min -= 20
        if V_min - 20 < 0: V_min = 0
        else: V_min -= 20
        lowerb = 'lowerb : (' + str(H_min) + ' ,' + str(S_min) + ' ,' + str(V_min) + ')'
        upperb = 'upperb : (' + str(H_max) + ' ,' + str(S_max) + ' ,' + str(V_max) + ')'
        txt1 = 'Learning ...'
        txt2 = 'OK !!!'
        if S_min < 5 or V_min < 5:
            cv.putText(img, txt1, (30, 50), cv.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
        else:
            cv.putText(img, txt2, (30, 50), cv.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        cv.putText(img, lowerb, (150, 30), cv.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
        cv.putText(img, upperb, (150, 50), cv.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
        hsv_range = ((int(H_min), int(S_min), int(V_min)), (int(H_max), int(S_max), int(V_max)))
        return img, hsv_range


class simplePID:
    '''very simple discrete PID controller'''

    def __init__(self, target, P, I, D):
        '''Create a discrete PID controller
        each of the parameters may be a vector if they have the same length
        Args:
        target (double) -- the target value(s)
        P, I, D (double)-- the PID parameter
        '''
        # check if parameter shapes are compatabile.
        if (not (np.size(P) == np.size(I) == np.size(D)) or ((np.size(target) == 1) and np.size(P) != 1) or (
                np.size(target) != 1 and (np.size(P) != np.size(target) and (np.size(P) != 1)))):
            raise TypeError('input parameters shape is not compatable')
        self.Kp = np.array(P)
        self.Ki = np.array(I)
        self.Kd = np.array(D)
        self.last_error = 0
        self.integrator = 0
        self.timeOfLastCall = None
        self.setPoint = np.array(target)
        self.integrator_max = float('inf')

    def update(self, current_value):
        '''Updates the PID controller.
        Args:
            current_value (double): vector/number of same legth as the target given in the constructor
        Returns:
            controll signal (double): vector of same length as the target
        '''
        current_value = np.array(current_value)
        if (np.size(current_value) != np.size(self.setPoint)):
            raise TypeError('current_value and target do not have the same shape')
        if (self.timeOfLastCall is None):
            # the PID was called for the first time. we don't know the deltaT yet
            # no controll signal is applied
            self.timeOfLastCall = time.perf_counter()
            return np.zeros(np.size(current_value))
        error = self.setPoint - current_value
        P = error
        currentTime = time.perf_counter()
        deltaT = (currentTime - self.timeOfLastCall)
        # integral of the error is current error * time since last update
        self.integrator = self.integrator + (error * deltaT)
        I = self.integrator
        # derivative is difference in error / time since last update
        D = (error - self.last_error) / deltaT
        self.last_error = error
        self.timeOfLastCall = currentTime
        # return controll signal
        return self.Kp * P + self.Ki * I + self.Kd * D
